#include <stdio.h>
#include "ipcModule.h"
#include "ipcMessage.h"
#include "ipcd.h"

IPC_EXPORT ipcModule **IPC_GetModuleList();

static const nsID TestModuleID =
{ /* e628fc6e-a6a7-48c7-adba-f241d1128fb8 */
    0xe628fc6e,
    0xa6a7,
    0x48c7,
    {0xad, 0xba, 0xf2, 0x41, 0xd1, 0x12, 0x8f, 0xb8}
};

class TestModule : public ipcModule
{
public:
    void Shutdown()
    {
        printf("*** TestModule::Shutdown\n");
    }

    const nsID &ID()
    {
        printf("*** TestModule::ID\n");
        return TestModuleID;
    }

    void HandleMsg(ipcClient *client, const ipcMessage *msg)
    {
        printf("*** TestModule::HandleMsg [%s]\n", msg->Data());
        ipcMessage *outMsg = new ipcMessage();
        static const char buf[] = "pong";
        outMsg->Init(TestModuleID, buf, sizeof(buf));
        IPC_SendMsg(client, outMsg);
    }
};

ipcModule **
IPC_GetModuleList()
{
    static TestModule testMod;
    static ipcModule *modules[2];

    modules[0] = &testMod;
    modules[1] = NULL;

    return modules;
}
