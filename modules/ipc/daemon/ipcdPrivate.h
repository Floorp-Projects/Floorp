#ifndef ipcdPrivate_h__
#define ipcdPrivate_h__

class ipcClient;

//
// upper limit on the number of active connections
// XXX may want to make this more dynamic
//
#define IPC_MAX_CLIENTS 100

//
// array of connected clients
//
extern ipcClient *ipcClients;
extern int        ipcClientCount;

//
// platform specific send message function.
//
PRStatus IPC_PlatformSendMsg(ipcClient *client, const ipcMessage *msg);

#endif // !ipcdPrivate_h__
