#ifndef ipcdUnix_h__
#define ipcdUnix_h__

//
// called by the ipcClient code to ensure that the client socket connection
// has PR_POLL_WRITE set.
//
void IPC_ClientWritePending(ipcClient *);

#endif // !ipcdUnix_h__
