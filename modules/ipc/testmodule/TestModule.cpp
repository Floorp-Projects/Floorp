#include <stdio.h>
#include "ipcModuleUtil.h"

#define TEST_MODULE_ID                                \
{ /* e628fc6e-a6a7-48c7-adba-f241d1128fb8 */          \
    0xe628fc6e,                                       \
    0xa6a7,                                           \
    0x48c7,                                           \
    {0xad, 0xba, 0xf2, 0x41, 0xd1, 0x12, 0x8f, 0xb8}  \
}
static const nsID kTestModuleID = TEST_MODULE_ID;

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

    static void HandleMsg(ipcClientHandle client,
                          const nsID     &target,
                          const void     *data,
                          PRUint32        dataLen)
    {
        printf("*** TestModule::HandleMsg [%s]\n", (const char *) data);

        static const char buf[] = "pong";
        IPC_SendMsg(client, kTestModuleID, buf, sizeof(buf));
    }

    static void ClientUp(ipcClientHandle client)
    {
        printf("*** TestModule::ClientUp [%u]\n", IPC_GetClientID(client));
    }

    static void ClientDown(ipcClientHandle client)
    {
        printf("*** TestModule::ClientDown [%u]\n", IPC_GetClientID(client));
    }
};

static ipcModuleMethods gTestMethods =
{
    IPC_MODULE_METHODS_VERSION,
    TestModule::Init,
    TestModule::Shutdown,
    TestModule::HandleMsg,
    TestModule::ClientUp,
    TestModule::ClientDown
};

static ipcModuleEntry gTestModuleEntry[] =
{
    { TEST_MODULE_ID, &gTestMethods }
};

IPC_IMPL_GETMODULES(TestModule, gTestModuleEntry)
