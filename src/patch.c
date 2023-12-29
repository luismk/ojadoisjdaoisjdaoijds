#include "patch.h"
#include "third_party/lend/ld32.h"

/**
 * Writes data into the remote process.
 */
VOID RemotePatch(HANDLE hProcess, DWORD dwAddr, PBYTE pbData, PBYTE pbBackup, DWORD cbData) {
    DWORD dwOldProtect;

    // Unprotect function.
    if (VirtualProtectEx(hProcess, (LPVOID)dwAddr, cbData, PAGE_EXECUTE_READWRITE, &dwOldProtect) == 0) {
        FatalError("Failed to remote patch: VirtualProtect failed. (%08x)", GetLastError());
    }

    // Optional: Backup remote memory.
    if (pbBackup) {
        if (!ReadProcessMemory(hProcess, (LPVOID)dwAddr, pbBackup, cbData, NULL)) {
            FatalError("Failed to remote patch: ReadProcessMemory failed. (%08x)", GetLastError());
        }
    }

    // Patch remote memory.
    if (!WriteProcessMemory(hProcess, (LPVOID)dwAddr, pbData, cbData, NULL)) {
        FatalError("Failed to remote patch: WriteProcessMemory failed. (%08x)", GetLastError());
    }

    // Re-protect function.
    if (VirtualProtectEx(hProcess, (LPVOID)dwAddr, cbData, dwOldProtect, &dwOldProtect) == 0) {
        FatalError("Failed to remote patch: VirtualProtect failed. (%08x)", GetLastError());
    }

    // Flush the icache. Otherwise, it is theoretically possible that our
    // patch will not work consistently.
    if (FlushInstructionCache(hProcess, (LPVOID)dwAddr, cbData) == 0) {
        FatalError("Failed to remote patch: FlushInstructionCache failed. (%08x)", GetLastError());
    }
}

/**
 * Installs a hook that redirects a function to a different function with a
 * compatible signature. This will destroy the first 6 bytes of the function,
 * so you will need to build a trampoline before installing the hook if you
 * want to be able to call the original function again.
 */
VOID InstallHook(PVOID pfnProc, PVOID pfnTargetProc) {
    DWORD dwOldProtect;
    DWORD dwRelAddr;
    CHAR pbHook[6] = {0xE9, 0x00, 0x00, 0x00, 0x00, 0xC3};

    // Unprotect function.
    if (VirtualProtect(pfnProc, 6, PAGE_EXECUTE_READWRITE, &dwOldProtect) == 0) {
        FatalError("Failed to install hook: VirtualProtect failed. (%08x)", GetLastError());
    }

    // Create and install hook.
    dwRelAddr = ((DWORD)pfnTargetProc - (DWORD)pfnProc - 5);
    memcpy(&pbHook[1], &dwRelAddr, 4);
    if (!WriteProcessMemory(GetCurrentProcess(), pfnProc, pbHook, 6, NULL)) {
        FatalError("Failed to install hook: WriteProcessMemory failed. (%08x)", GetLastError());
    }

    // Re-protect function.
    if (VirtualProtect(pfnProc, 6, dwOldProtect, &dwOldProtect) == 0) {
        FatalError("Failed to install hook: VirtualProtect failed. (%08x)", GetLastError());
    }

    // Flush the icache. Otherwise, it is theoretically possible that our
    // patch will not work consistently.
    if (FlushInstructionCache(GetCurrentProcess(), pfnProc, 6) == 0) {
        FatalError("Failed to install hook: FlushInstructionCache failed. (%08x)", GetLastError());
    }
}

/**
 * Counts at least minBytes bytes worth of instructions, ending at an
 * instruction boundary.
 */
DWORD CountOpcodeBytes(FARPROC fn, DWORD minBytes) {
    PBYTE fndata = (PBYTE)fn;
    DWORD count = 0;

    while (count < minBytes) {
        count += length_disasm(&fndata[count]);
    }

    return count;
}

/**
 * Builds a trampoline to be able to call a function that has been hooked.
 * Conceptually, it takes prefixLen bytes (at least 6 bytes) from the original
 * function, then places them into a small block of code that jumps that many
 * bytes into the original function call. prefixLen needs to be longer than 6,
 * but must end on an instruction boundary. In addition, this approach is
 * naive and won't always work well, though for most functions the prefix will
 * be in the prologue and should be safe.
 */
PCHAR BuildTrampoline(DWORD fn, DWORD prefixLen) {
    DWORD trampolineLen;
    DWORD oldProtect;
    DWORD relAddr;
    PCHAR codeblock;

    // Allocate a block of memory. We need to fit the prefix + a jump.
    // Extra byte is for a return so that the instruction after the jump will
    // be valid. I'm not sure if this is strictly necessary.
    trampolineLen = prefixLen + 6;
    codeblock = LocalAlloc(0, trampolineLen);

    // Copy the prefix into our newly minted codeblock.
    memcpy(codeblock, (void *)fn, prefixLen);

    // Calculate the jump address. We want to jump into the function at the same
    // point we left off here, so we can just subtract the size of the jmp
    // instruction and otherwise it's a flat subtraction.
    relAddr = (DWORD)fn - (DWORD)codeblock - 5;

    // Create our jump instruction.
    codeblock[prefixLen] = 0xE9;
    memcpy(&codeblock[prefixLen + 1], &relAddr, 4);

    // ...and a return at the end, for good measure.
    codeblock[prefixLen + 5] = 0xC3;

    // Mark the codeblock as executable.
    VirtualProtect(codeblock, trampolineLen, PAGE_EXECUTE_READWRITE, &oldProtect);

    return codeblock;
}

/**
 * Creates a trampoline then hooks the provided function to call the target
 * instead.
 */
PVOID HookFunc(PVOID pfnProc, PVOID pfnTargetProc) {
    DWORD prefixLen, trampoline;

    prefixLen = CountOpcodeBytes(pfnProc, 6);

    trampoline = (DWORD)BuildTrampoline((DWORD)pfnProc, prefixLen);

    InstallHook((PCHAR)pfnProc, pfnTargetProc);

    return (PVOID)trampoline;
}

/**
 * Hooks a procedure in a module.
 */
PVOID HookProc(HMODULE hModule, LPCSTR szName, PVOID pfnTargetProc) {
    PVOID pfnLibraryProc, pfnTrampolineProc;

    pfnLibraryProc = GetProc(hModule, szName);

    pfnTrampolineProc = HookFunc(pfnLibraryProc, pfnTargetProc);

    return pfnTrampolineProc;
}
