#include <stdio.h>
#include "ipcModule.h"
#include "ipcMessage.h"

IPC_EXPORT int IPC_GetModules(ipcDaemonMethods *, ipcModuleEntry **);

static const nsID TestModuleID =
{ /* e628fc6e-a6a7-48c7-adba-f241d1128fb8 */
    0xe628fc6e,
    0xa6a7,
    0x48c7,
    {0xad, 0xba, 0xf2, 0x41, 0xd1, 0x12, 0x8f, 0xb8}
};

static ipcDaemonMethods *gDaemonMethods;

struct TestModule
{
    static void Shutdown()
    {
        printf("*** TestModule::Shutdown\n");
    }

    static void HandleMsg(ipcClientHandle client, const ipcMessage *msg)
    {
        printf("*** TestModule::HandleMsg [%s]\n", msg->Data());
        ipcMessage outMsg;
        static const char buf[] = "pong";
        outMsg.Init(TestModuleID, buf, sizeof(buf));
        gDaemonMethods->sendMsg(client, &outMsg);
    }
};

int
IPC_GetModules(ipcDaemonMethods *daemonMeths, ipcModuleEntry **entries)
{
    printf("*** testmodule: IPC_GetModules\n");

    static ipcModuleMethods methods =
    {
        IPC_MODULE_METHODS_VERSION,
        TestModule::Shutdown,
        TestModule::HandleMsg
    };
    static ipcModuleEntry moduleEntry =
    {
        TestModuleID,
        &methods
    };

    gDaemonMethods = daemonMeths;

    *entries = &moduleEntry;
    return 1;
}
