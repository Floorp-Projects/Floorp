#ifndef ipcModuleUtil_h__
#define ipcModuleUtil_h__

#include "prlog.h"
#include "ipcModule.h"

extern ipcDaemonMethods *gIPCDaemonMethods;

//-----------------------------------------------------------------------------
// inline wrapper functions
//-----------------------------------------------------------------------------

inline PRStatus
IPC_DispatchMsg(ipcClientHandle client, const ipcMessage *msg)
{
    PR_ASSERT(gIPCDaemonMethods);
    return gIPCDaemonMethods->dispatchMsg(client, msg);
}

inline PRStatus
IPC_SendMsg(ipcClientHandle client, const ipcMessage *msg)
{
    PR_ASSERT(gIPCDaemonMethods);
    return gIPCDaemonMethods->sendMsg(client, msg);
}

inline ipcClientHandle
IPC_GetClientByID(PRUint32 id)
{
    PR_ASSERT(gIPCDaemonMethods);
    return gIPCDaemonMethods->getClientByID(id);
}

inline ipcClientHandle
IPC_GetClientByName(const char *name)
{
    PR_ASSERT(gIPCDaemonMethods);
    return gIPCDaemonMethods->getClientByName(name);
}

inline void
IPC_EnumClients(ipcClientEnumFunc func, void *closure)
{
    PR_ASSERT(gIPCDaemonMethods);
    gIPCDaemonMethods->enumClients(func, closure);
}

inline PRUint32
IPC_GetClientID(ipcClientHandle client)
{
    PR_ASSERT(gIPCDaemonMethods);
    return gIPCDaemonMethods->getClientID(client);
}

inline const char *
IPC_GetPrimaryClientName(ipcClientHandle client)
{
    PR_ASSERT(gIPCDaemonMethods);
    return gIPCDaemonMethods->getPrimaryClientName(client);
}

inline PRBool
IPC_ClientHasName(ipcClientHandle client, const char *name)
{
    PR_ASSERT(gIPCDaemonMethods);
    return gIPCDaemonMethods->clientHasName(client, name);
}

inline PRBool
IPC_ClientHasTarget(ipcClientHandle client, const nsID &target)
{
    PR_ASSERT(gIPCDaemonMethods);
    return gIPCDaemonMethods->clientHasTarget(client, target);
}

inline void
IPC_EnumClientNames(ipcClientHandle client, ipcClientNameEnumFunc func, void *closure)
{
    PR_ASSERT(gIPCDaemonMethods);
    gIPCDaemonMethods->enumClientNames(client, func, closure);
}

inline void
IPC_EnumClientTargets(ipcClientHandle client, ipcClientTargetEnumFunc func, void *closure)
{
    PR_ASSERT(gIPCDaemonMethods);
    gIPCDaemonMethods->enumClientTargets(client, func, closure);
}

//-----------------------------------------------------------------------------
// inline composite functions
//-----------------------------------------------------------------------------

inline PRStatus
IPC_SendMsg(PRUint32 clientID, ipcMessage *msg)
{
    ipcClient *client = IPC_GetClientByID(clientID);
    if (!client)
        return PR_FAILURE;
    return IPC_SendMsg(client, msg);
}

//-----------------------------------------------------------------------------
// module factory macros
//-----------------------------------------------------------------------------

#define IPC_IMPL_GETMODULES(_modName, _modEntries)                        \
    ipcDaemonMethods *gIPCDaemonMethods;                                  \
    IPC_EXPORT int IPC_GetModules(ipcDaemonMethods *, ipcModuleEntry **); \
    int IPC_GetModules(ipcDaemonMethods *dmeths, ipcModuleEntry **ents) { \
        gIPCDaemonMethods = dmeths;                                       \
        *ents = _modEntries;                                              \
        return sizeof(_modEntries) / sizeof(ipcModuleEntry);              \
    }

#endif // !ipcModuleUtil_h__
