#include "ipcConfig.h"
#include "ipcLog.h"
#include "ipcTransport.h"

//-----------------------------------------------------------------------------
// windows specific ipcTransport impl
//-----------------------------------------------------------------------------

nsresult
ipcTransport::Shutdown()
{
    LOG(("ipcTransport::Shutdown\n"));

    mHaveConnection = PR_FALSE;
    return NS_OK;
}

nsresult
ipcTransport::Connect()
{
    LOG(("ipcTransport::Connect\n"));
    return NS_OK;
}

nsresult
ipcTransport::SendMsg_Internal(ipcMessage *msg)
{
    LOG(("ipcTransport::SendMsg_Internal\n"));
    return NS_OK;
}

