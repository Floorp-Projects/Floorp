#ifdef XP_UNIX
#include <sys/types.h>
#include <unistd.h>
#endif

#include "prenv.h"
#include "prprf.h"
#include "plstr.h"
#include "ipcLog.h"

PRBool ipcLogEnabled;
char ipcLogPrefix[10];

void
IPC_InitLog(const char *prefix)
{
    if (PR_GetEnv("IPC_LOG_ENABLE")) {
        ipcLogEnabled = PR_TRUE;
        PL_strncpyz(ipcLogPrefix, prefix, sizeof(ipcLogPrefix));
    }
}

void
IPC_Log(const char *fmt, ... )
{
    va_list ap;
    va_start(ap, fmt);
    PRUint32 nb;
    char buf[512];

    if (ipcLogPrefix[0])
#ifdef XP_UNIX
        nb = PR_snprintf(buf, sizeof(buf), "[%u] %s ", getpid(), ipcLogPrefix);
#else
        nb = PR_snprintf(buf, sizeof(buf), "%s ", ipcLogPrefix);
#endif
    else
        nb = 0;

    PR_vsnprintf(buf + nb, sizeof(buf) - nb, fmt, ap);
    buf[sizeof(buf) - 1] = '\0';

    printf("%s", buf);

    va_end(ap);
}
