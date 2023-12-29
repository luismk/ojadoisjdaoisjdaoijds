#include "redir.h"
#include "../../patch.h"
#include "../../config.h"

HMODULE hWinsock = NULL;
PFNCONNECTPROC pConnect = NULL;
PFNHTONSPROC pHtons = NULL;
PFNGETADDRINFO pGetAddrInfo = NULL;
PFNFREEADDRINFO pFreeAddrInfo = NULL;
struct in_addr connectAddr = {127, 0, 0, 1};

/**
 * ConnectHook redirects Winsock TCP sockets to localhost.
 */
int STDCALL ConnectHook(SOCKET s, const struct sockaddr *name, int namelen) {
    struct sockaddr_in name_in = *(struct sockaddr_in*)name;
    struct sockaddr_in override_in;
    char *oldaddr = (char*)&name_in.sin_addr;
    char *newaddr = (char*)&override_in.sin_addr;
    memcpy(&override_in, &name_in, sizeof(struct sockaddr_in));

    if (RewriteAddr(&override_in)) {
        Log("connect(0x%08x, %d.%d.%d.%d:%d => %d.%d.%d.%d:%d)\r\n", s,
            oldaddr[0], oldaddr[1], oldaddr[2], oldaddr[3], pHtons(name_in.sin_port),
            newaddr[0], newaddr[1], newaddr[2], newaddr[3], pHtons(override_in.sin_port));
    } else {
        Log("connect(0x%08x, %d.%d.%d.%d:%d) // (no rewrite rules matched)\r\n", s,
            newaddr[0], newaddr[1], newaddr[2], newaddr[3], pHtons(override_in.sin_port));
    }

    return pConnect(s, (struct sockaddr*)&override_in, sizeof(struct sockaddr_in));
}

VOID InitRedirHook() {
    hWinsock = LoadLib("ws2_32");
    pConnect = HookProc(hWinsock, "connect", ConnectHook);
    pHtons = GetProc(hWinsock, "htons");
    pGetAddrInfo = GetProc(hWinsock, "getaddrinfo");
    pFreeAddrInfo = GetProc(hWinsock, "freeaddrinfo");
}
