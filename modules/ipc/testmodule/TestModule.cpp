#include <stdio.h>
#include "ipcModuleUtil.h"
#include "ipcMessage.h"

static const nsID TestModuleID =
{ /* e628fc6e-a6a7-48c7-adba-f241d1128fb8 */
    0xe628fc6e,
    0xa6a7,
    0x48c7,
    {0xad, 0xba, 0xf2, 0x41, 0xd1, 0x12, 0x8f, 0xb8}
};

struct TestModule
{
    static void Init()
    {
        printf("*** TestModule::Init\n");
    }

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
        IPC_SendMsg(client, &outMsg);
    }
};

static ipcModuleMethods gTestMethods =
{
    IPC_MODULE_METHODS_VERSION,
    TestModule::Init,
    TestModule::Shutdown,
    TestModule::HandleMsg
};

static ipcModuleEntry gTestModuleEntry[] =
{
    { TestModuleID, &gTestMethods }
};

IPC_IMPL_GETMODULES(TestModule, gTestModuleEntry)
