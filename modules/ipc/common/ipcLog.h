#ifndef ipcLog_h__
#define ipcLog_h__

#include "prtypes.h"

extern PRBool ipcLogEnabled;
extern void IPC_InitLog(const char *prefix);
extern void IPC_Log(const char *fmt, ...);

#define IPC_LOG(_args)         \
    PR_BEGIN_MACRO             \
        if (ipcLogEnabled)     \
            IPC_Log _args;     \
    PR_END_MACRO

#define LOG(args)     IPC_LOG(args)
#define LOG_ENABLED() ipcLogEnabled

#endif // !ipcLog_h__
