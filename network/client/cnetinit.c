

#include "xp.h"
#include "netutils.h"
#include "mkselect.h"
#include "mktcp.h"
#include "mkgeturl.h"

#include "fileurl.h"
#include "httpurl.h"
#include "ftpurl.h"
#include "abouturl.h"
#include "gophurl.h"
#include "jsurl.h"
#include "fileurl.h"
#include "remoturl.h"
#include "dataurl.h"
#include "netcache.h"


PUBLIC void
NET_ClientProtocolInitialize(void)
{

        NET_InitFileProtocol();
        NET_InitHTTPProtocol();
        NET_InitMemCacProtocol();
        NET_InitFTPProtocol();
        NET_InitAboutProtocol();
        NET_InitGopherProtocol();
        NET_InitMochaProtocol();
        NET_InitRemoteProtocol();
        NET_InitDataURLProtocol();

#ifdef JAVA
        NET_InitMarimbaProtocol();
#endif
}
