#include "common.h"

#define LOG_FILENAME "gglog.txt"

PSTR pszSelfPath = NULL;
PSTR pszSelfName = NULL;
PSTR pszLogPrefix = NULL;

int strcmp(LPCSTR dest, LPCSTR src) {
    int cmp;
    do {
        cmp = *dest - *src;
        if (cmp > 0) {
            return 1;
        }
        else if (cmp < 0) {
            return -1;
        }
    } while (*dest++ && *src++);
    return 0;
}

int memcmp(LPCVOID dest, LPCVOID src, size_t size) {
    LPCSTR bdest = (LPCSTR)dest;
    LPCSTR bsrc = (LPCSTR)src;
    int cmp;
    while (size > 0) {
        cmp = *bdest++ - *bsrc++;
        if (cmp > 0) {
            return 1;
        }
        else if (cmp < 0) {
            return -1;
        }
        --size;
    }
    return 0;
}

PVOID memcpy(PVOID dest, VOID const* src, size_t size) {
    BYTE* bdest = (BYTE*)dest;
    BYTE const* bsrc = (BYTE const*)src;
    while (size > 0) {
        *bdest++ = *bsrc++;
        --size;
    }
    return dest;
}

PVOID memset(PVOID p, INT c, UINT size) {
    PCHAR data = (PCHAR)p;
    while (size > 0) {
        *data++ = c;
        --size;
    }
    return p;
}

PSTR GetSelfPath() {
    if (pszSelfPath != NULL) {
        return pszSelfPath;
    }

    pszSelfPath = LocalAlloc(0, 4096);

    GetModuleFileNameA(GetModuleHandleA("ijl15"), pszSelfPath, 4096);

    return pszSelfPath;
}

BOOL FileExists(LPCTSTR szPath) {
    return GetFileAttributes(szPath) != INVALID_FILE_ATTRIBUTES;
}

LPSTR ReadEntireFile(LPCSTR szPath) {
    HANDLE hFile = NULL;
    DWORD dwBytesRead = 0, dwBytesToRead = 0;
    BOOL bErrorFlag = FALSE;
    LPSTR buffer = NULL, data = NULL;

    hFile = CreateFileA(szPath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        FatalError("Error opening file %s.", szPath);
        ExitProcess(1);
        return NULL;
    }

    dwBytesToRead = GetFileSize(hFile, NULL);
    buffer = LocalAlloc(0, dwBytesToRead + 1);
    memset(buffer, 0, dwBytesToRead + 1);

    data = buffer;
    while (dwBytesToRead > 0) {
        bErrorFlag = ReadFile(hFile, data, dwBytesToRead, &dwBytesRead, NULL);

        if (!bErrorFlag) {
            FatalError("Error loading file %s.", szPath);
            break;
        }

        data += dwBytesRead;
        dwBytesToRead -= dwBytesRead;
    }

    CloseHandle(hFile);
    return buffer;
}

VOID WriteEntireFile(LPCSTR szPath, LPCSTR data, DWORD dwBytesToWrite) {
    HANDLE hFile = NULL;
    DWORD dwBytesWritten;
    BOOL bErrorFlag = FALSE;

    hFile = CreateFile(szPath, FILE_WRITE_DATA, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        FatalError("Error creating file %s.", szPath);
        return;
    }

    while (dwBytesToWrite > 0) {
        bErrorFlag = WriteFile(hFile, data, dwBytesToWrite, &dwBytesWritten, NULL);

        if (!bErrorFlag) {
            FatalError("Error writing to file %s.", szPath);
            break;
        }

        data += dwBytesWritten;
        dwBytesToWrite -= dwBytesWritten;
    }

    CloseHandle(hFile);
}

VOID FatalError(PCHAR fmt, ...) {
    PCHAR buffer = LocalAlloc(0, 4096);

    va_list args;
    va_start(args, fmt);
    wvsprintfA(buffer, fmt, args);
    va_end(args);

    MessageBoxA(HWND_DESKTOP, buffer, "rugburn", MB_OK | MB_ICONERROR);
    LocalFree(buffer);
    ExitProcess(1);
}

VOID Warning(PCHAR fmt, ...) {
    va_list args;
    PCHAR buffer = LocalAlloc(0, 4096);

    va_start(args, fmt);
    wvsprintfA(buffer, fmt, args);
    va_end(args);

    MessageBoxA(HWND_DESKTOP, buffer, "rugburn", MB_OK | MB_ICONWARNING);
    LocalFree(buffer);
}

extern PFNCREATEFILEAPROC pCreateFileA;

VOID Log(PCHAR fmt, ...) {
    va_list args;
    HANDLE hAppend;
    PCHAR buffer = LocalAlloc(0, 4096);
    PCHAR pfxbuffer = LocalAlloc(0, 128);
    PCHAR logmsg = buffer;
    DWORD cb = 0;

    if (pszLogPrefix != NULL) {
        StrCpyA(logmsg, pszLogPrefix);
        while (*logmsg != '\0') logmsg++;
    }

    wsprintfA(pfxbuffer, "T:%d] ", GetCurrentThreadId());
    StrCpyA(logmsg, pfxbuffer);
    while (*logmsg != '\0') logmsg++;

    cb = logmsg - buffer;

    va_start(args, fmt);
    cb += wvsprintfA(logmsg, fmt, args);
    va_end(args);

    hAppend = CreateFileA(LOG_FILENAME, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    WriteFile(hAppend, buffer, cb, &cb, NULL);
    CloseHandle(hAppend);
    LocalFree(buffer);
}

VOID LogInit() {
    PCHAR szPfxBuf = LocalAlloc(0, 4096);
    PCHAR szFileName = LocalAlloc(0, 4096);
    PCHAR szBaseName;
    DWORD cbFileName;

    cbFileName = GetModuleFileNameA(NULL, szFileName, 4096);
    szBaseName = szFileName + cbFileName;

    while (szBaseName > szFileName) {
        if (*szBaseName == '\\') {
            szBaseName++;
            break;
        }
        szBaseName--;
    }

    pszSelfName = szBaseName;
    wsprintfA(szPfxBuf, "%s:%d] ", szBaseName, GetCurrentProcessId());
    pszLogPrefix = szPfxBuf;
}

HMODULE LoadLib(LPCSTR szName) {
    HMODULE hModule;

    hModule = LoadLibraryA(szName);
    if (hModule == NULL) {
        FatalError("Could not load module %s (%08x)", szName, GetLastError());
    }

    return hModule;
}

PVOID GetProc(HMODULE hModule, LPCSTR szName) {
    PVOID pvProc;

    pvProc = (PVOID)GetProcAddress(hModule, szName);
    if (pvProc == NULL) {
        FatalError("Could not load proc %s (%08x)", szName, GetLastError());
    }

    return pvProc;
}

PANGYAVER DetectPangyaVersion() {
    if (FileExists("PangyaUS.ini")) {
        return PANGYA_US;
    }
    else if (FileExists("PangyaJP.ini")) {
        return PANGYA_JP;
    }
    else if (FileExists("PangyaTH.ini")) {
        return PANGYA_TH;
    }
    return PANGYA_US;
}

PSTR GetPangyaArg(PANGYAVER pangyaVersion) {
    switch (pangyaVersion) {
    case PANGYA_US:
        return StrDupA("{2D0FA24B-336B-4e99-9D30-3116331EFDA0}");

    case PANGYA_JP:
        return StrDupA("{E69B65A2-7A7E-4977-85E5-B19516D885CB}");

    case PANGYA_TH:
        return StrDupA("{E69B65A2-7A7E-4977-85E5-B19516D885CB}");

    default:
        return NULL;
    }
}