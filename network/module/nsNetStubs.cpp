/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsNetStubs.h"

#include "nspr.h"
#include "xp_hash.h"
#include "xp_file.h"
#include "libi18n.h"
#include "libevent.h"
#include "mkgeturl.h"
#include "net.h"
#include "nsIRefreshUrl.h"
#include "nsString.h"
#include "nsNetStream.h"
#include "intl_csi.h"

#ifdef NS_NET_FILE

extern char *USER_DIR;
extern char *CACHE_DIR;
extern char *DEF_DIR;

// For xp to ns file translation
#include "nsINetFile.h"
#include "nsVoidArray.h"
#include "direct.h"

// The nsINetfile
static nsINetFile *fileMgr = nsnull;

typedef struct _xp_to_nsFile {
    nsFile *nsFp;
    XP_File xpFp;
} xpNSFile;

// Array of translation structs from xp to ns file.
static nsVoidArray switchBack;

// end xp to ns file translation

#endif // NS_NET_FILE

extern "C" {
#include "secnav.h"
#include "preenc.h"
}

/* From libimg */
#define OPAQUE_CONTEXT void

/*
 *---------------------------------------------------------------------------
 * From ns/lib/libi18n/intl_csi.c
 *---------------------------------------------------------------------------
 */

extern "C" {

#if defined(XP_UNIX) || defined(XP_MAC)
DB *
dbopen(const char *fname, int flags,int mode, DBTYPE type, 
       const void *openinfo)
{
#if defined(XP_MAC)
	PR_ASSERT(FALSE);
#endif

    return NULL;
}
#endif

/*  Meta charset is weakest. Only set doc_csid if no http or override */
void
INTL_SetCSIDocCSID (INTL_CharSetInfo c, int16 doc_csid)
{
    MOZ_FUNCTION_STUB;
}


int16
INTL_GetCSIDocCSID(INTL_CharSetInfo c)
{
    MOZ_FUNCTION_STUB;
    return 0;
}


void
INTL_SetCSIWinCSID(INTL_CharSetInfo c, int16 win_csid)
{
    MOZ_FUNCTION_STUB;
}


int16
INTL_GetCSIWinCSID(INTL_CharSetInfo c)
{
    MOZ_FUNCTION_STUB;
    return 0;
}


INTL_CharSetInfo 
LO_GetDocumentCharacterSetInfo(MWContext *context)
{
    MOZ_FUNCTION_STUB;
    return NULL;
}


/*
 *---------------------------------------------------------------------------
 * From ns/lib/libi18n/acptlang.c
 *---------------------------------------------------------------------------
 */

/* INTL_GetAcceptCharset()                          */
/* return the AcceptCharset from XP Preference      */
/* this should be a C style NULL terminated string  */
char* INTL_GetAcceptCharset()
{
    MOZ_FUNCTION_STUB;
    return NULL;
}


/* INTL_GetAcceptLanguage()                         */
/* return the AcceptLanguage from XP Preference     */
/* this should be a C style NULL terminated string  */
char* INTL_GetAcceptLanguage()
{
    MOZ_FUNCTION_STUB;
    return NULL;
}




/*
 *---------------------------------------------------------------------------
 * From ns/lib/libi18n/net_junk.c
 *---------------------------------------------------------------------------
 */
PUBLIC Stream *
INTL_ConvCharCode (int         format_out,
                   void       *data_obj,
                   URL_Struct *URL_s,
                   MWContext  *mwcontext)
{
    MOZ_FUNCTION_STUB;
    return NULL;
}



/*
 *---------------------------------------------------------------------------
 * From ns/lib/libi18n/cvchcode.c
 *---------------------------------------------------------------------------
 */

/* INTL_GetCharCodeConverter:
 * RETURN: 1 if converter found, else 0
 * Also, sets:
 *              obj->cvtfunc:   function handle for chararcter
 *                              code set streams converter
 *              obj->cvtflag:   (Optional) flag to converter
 *                              function
 *              obj->from_csid: Code set converting from
 *              obj->to_csid:   Code set converting to
 * If the arg to_csid==0, then use the the conversion  for the
 * first conversion entry that matches the from_csid.
 */
int
INTL_GetCharCodeConverter(register int16  from_csid,
                          register int16  to_csid,
                          CCCDataObject   obj)
{
    MOZ_FUNCTION_STUB;
    return 0;
}


CCCDataObject
INTL_CreateCharCodeConverter()
{
    MOZ_FUNCTION_STUB;
    return NULL;
}


unsigned char *
INTL_CallCharCodeConverter(CCCDataObject obj, const unsigned char *buf,
                           int32 bufsz)
{
    MOZ_FUNCTION_STUB;
    return NULL;
}


void
INTL_DestroyCharCodeConverter(CCCDataObject obj)
{
    MOZ_FUNCTION_STUB;
}

/*
 *---------------------------------------------------------------------------
 * From ns/lib/libi18n/csnamefn.c
 *---------------------------------------------------------------------------
 */
int16
INTL_CharSetNameToID(char	*charset)
{
    MOZ_FUNCTION_STUB;
    return 0;
}


/*
 *---------------------------------------------------------------------------
 * From ns/lib/libi18n/ucs2.c
 *---------------------------------------------------------------------------
 */
uint32    INTL_TextToUnicode(
    INTL_Encoding_ID encoding,
    unsigned char*   src,
    uint32           srclen,
    INTL_Unicode*    ustr,
    uint32           ubuflen
)
{
    MOZ_FUNCTION_STUB;
    return 0;
}


uint32    INTL_TextToUnicodeLen(
    INTL_Encoding_ID encoding,
    unsigned char*   src,
    uint32           srclen
)
{
    MOZ_FUNCTION_STUB;
    return 0;
}

/*
 *---------------------------------------------------------------------------
 * From ns/lib/libi18n/fe_ccc.c
 *---------------------------------------------------------------------------
 */
/*
 INTL_DocToWinCharSetID,
   Based on DefaultDocCSID, it determines which Win CSID to use for Display
*/
/*

        To Do: (ftang)

        We should seperate the DocToWinCharSetID logic from the cscvt_t table
        for Cyrillic users. 

*/
int16 INTL_DocToWinCharSetID(int16 csid)
{
    MOZ_FUNCTION_STUB;
    return CS_FE_ASCII;
}


/*
 *---------------------------------------------------------------------------
 * From ns/lib/libi18n/doc_ccc.c
 *---------------------------------------------------------------------------
 */

PUBLIC void INTL_CCCReportMetaCharsetTag(MWContext *context, char *charset_tag)
{
    MOZ_FUNCTION_STUB;
}

/*
 *---------------------------------------------------------------------------
 * From ns/cmd/winfe/authdll.cpp
 *---------------------------------------------------------------------------
 */
char * WFE_BuildCompuserveAuthString(URL_Struct *URL_s)
{
    MOZ_FUNCTION_STUB;
    return NULL;
}


int
WFE_DoCompuserveAuthenticate(MWContext *context,
                             URL_Struct *URL_s, 
                             char *authenticate_header_value)
{
    MOZ_FUNCTION_STUB;
    return NET_AUTH_FAILED_DONT_DISPLAY;
}



/*
 *---------------------------------------------------------------------------
 * From ns/cmd/winfe/regproto.cpp
 *---------------------------------------------------------------------------
 */

//  Purpose:    See if we're supposed to handle a specific protocol differently.
//  Arguments:  pContext        The context.
//              iFormatOut      The format out (possibly saving).
//              pURL            The URL to load.
//              pExitFunc       The URL exit routine.  If we handle the protocol, we use the function.
//Returns:      BOOL            TRUE    We want to handle the protocol here.
//                              FALSE   Netlib continues to handle the protocol.
//Comments:     To work with certain DDE topics which can cause URLs of a specific protocol type to be handled by another
//              application.
//Revision History:
//      01-17-95        created GAB
//

XP_Bool FE_UseExternalProtocolModule(MWContext *pContext, FO_Present_Types iFormatOut, URL_Struct *pURL,
                                     Net_GetUrlExitFunc *pExitFunc)	
{
    MOZ_FUNCTION_STUB;
    return FALSE;
}



/*
 *---------------------------------------------------------------------------
 * From ns/cmd/winfe/femess.cpp
 *---------------------------------------------------------------------------
 */
//
// Prompt the user for a local file name
//
int FE_PromptForFileName(MWContext *context,
                         const char *prompt_string,
                         const char *default_path,
                         XP_Bool file_must_exist_p,
                         XP_Bool directories_allowed_p,
                         ReadFileNameCallbackFunction fn,
                         void *closure)
{
    MOZ_FUNCTION_STUB;
    return -1;
}



/*
 *---------------------------------------------------------------------------
 * From ns/cmd/winfe/fegui.cpp
 *---------------------------------------------------------------------------
 */
//  Return some spanked out full path on the mac.
//  For windows, we just return what was passed in.
//  We must provide it in a seperate buffer, otherwise they might change
//      the original and change also what they believe to be saved.
char *WH_FilePlatformName(const char *pName)
{
    MOZ_FUNCTION_STUB;
    return NULL;
}


//  Purpose:    Set a delayed load timer for a window for a particular URL.
//  Arguments:  pContext    The context that we're in.  We are only interested on wether or not there appears to be a Frame/hence document.
//              ulSeconds   The number of seconds that will pass before we attempt the reload.
//  Returns:    void
//  Comments:
//  Revision History:
//  02-17-95    created GAB
//  07-22-95    modified to use new context.
//  05-03-96    modified to use new API outside of idle loop.
//

NS_DEFINE_IID(kRefreshURLIID,       NS_IREFRESHURL_IID);

void FE_SetRefreshURLTimer(MWContext *pContext, URL_Struct *URL_s) 
{
  nsresult rv;
  nsIRefreshUrl* IRefreshURL=nsnull;
  nsString refreshURL(URL_s->refresh_url);
  nsConnectionInfo* pConn;

  NS_PRECONDITION((URL_s != nsnull), "Null pointer...");
  NS_PRECONDITION((pContext != nsnull), "Null pointer...");
  NS_PRECONDITION((pContext->modular_data != nsnull), "Null pointer...");
  NS_PRECONDITION((pContext->modular_data->fe_data != nsnull), "Null pointer...");

  // Get the nsConnectionInfo out of the context.
  // modular_data points to a URL_Struct.
  pConn = (nsConnectionInfo*) pContext->modular_data->fe_data;

  NS_PRECONDITION((pConn != nsnull), "Null pointer...");

  if (nsnull != pConn) {
    /* Get the pointer to the nsIRefreshURL from the nsISupports
     * of the nsIContentViewerContainer the nsConnnectionInfo holds.
     */
    if (nsnull != pConn->pURL) {
      nsISupports* container;

      rv = pConn->pURL->GetContainer(&container);
      if (container && (rv == NS_OK)) {
        rv = container->QueryInterface(kRefreshURLIID, (void**)&IRefreshURL);
        if(NS_SUCCEEDED(rv)) {
          nsIURL* newURL;

          rv = NS_NewURL(&newURL, refreshURL);
          if (NS_SUCCEEDED(rv)) {
            rv = IRefreshURL->RefreshURL(newURL, URL_s->refresh*1000, FALSE);
            NS_RELEASE(newURL);
          }
          NS_RELEASE(IRefreshURL);
        }
        NS_RELEASE(container);
      }
    }
  }
  MOZ_FUNCTION_STUB;
}


PUBLIC void 
FE_ConnectToRemoteHost(MWContext * context, int url_type, char *
    hostname, char * port, char * username) 
{
    MOZ_FUNCTION_STUB;
}


/*
 *---------------------------------------------------------------------------
 * From ns/cmd/winfe/feutil.cpp
 *---------------------------------------------------------------------------
 */

//      The purpose of FEU_AhAhAhAhStayingAlive is to house the one and only
//      saturday night fever function; named after Chouck's idol.
//      This function will attempt to do all that is necessary in order
//      to keep the application's messages flowing and idle loops
//      going when we need to finish an asynchronous operation
//      synchronously.
//      The current cases that cause this are RPC calls into the
//      application where we need to return a value or produce output
//      from only one entry point before returning to the caller.
//
//      If and when you modify this function, get your changes reviewed.
//      It is too vital that this work, always.
//
//      The function only attempts to look at one message at a time, or
//      propigate one idle call at a time, keeping it's own idle count.
//      This is not a loop.  YOU must provide the loop which calls this function.
//
//      Due to the nature and order of which we process windows messages, this
//      can seriously mess with the flow of control through the client.
//      If there is any chance at all that you can ensure that you are at the
//      bottom of the message queue before doing this, then please take those
//      measures.
void FEU_StayingAlive()
{
    MOZ_FUNCTION_STUB;
}




/*
 *---------------------------------------------------------------------------
 * From ns/cmd/winfe/cfe.cpp
 *---------------------------------------------------------------------------
 */
void FE_RunNetcaster(MWContext *context) 
{
    MOZ_FUNCTION_STUB;
}

/*
 *---------------------------------------------------------------------------
 * From ns/lib/xp/xp_cntxt.c
 *---------------------------------------------------------------------------
 */

/*
 * if the passed context is in the global context list
 * TRUE is returned.  Otherwise false
 */
Bool XP_IsContextInList(MWContext *context)
{
    MOZ_FUNCTION_STUB;
    return FALSE;
}

/*
 *---------------------------------------------------------------------------
 * From ns/lib/xp/xplocale.c
 *---------------------------------------------------------------------------
 */

size_t XP_StrfTime(MWContext* context, char *result, size_t maxsize, int format,
                   const struct tm *timeptr)
{
    MOZ_FUNCTION_STUB;
    return 0;
}


/*
 *---------------------------------------------------------------------------
 * From ns/lib/layout/edtutil.cpp
 *---------------------------------------------------------------------------
 */

void EDT_SavePublishUsername(MWContext *pContext, char *pAddress, char *pUsername)
{
    MOZ_FUNCTION_STUB;
}

/*
 *---------------------------------------------------------------------------
 * From ns/lib/layout/layutil.c
 *---------------------------------------------------------------------------
 */

 /*
 * Return the width of the window for this context in chars in the default
 * fixed width font.  Return -1 on error.
 */
int16
LO_WindowWidthInFixedChars(MWContext *context)
{
    MOZ_FUNCTION_STUB;
    return 80;
}



/*
 *---------------------------------------------------------------------------
 * From ns/lib/layout/layinfo.c
 *---------------------------------------------------------------------------
 */

/*
 * Prepare a bunch of information about the content of this
 * document and prints the information as HTML down
 * the passed in stream.
 *
 * Returns:
 *      -1      If the context passed does not correspond to any currently
 *              laid out document.
 *      0       If the context passed corresponds to a document that is
 *              in the process of being laid out.
 *      1       If the context passed corresponds to a document that is
 *              completly laid out and info can be found.
 */
PUBLIC intn
LO_DocumentInfo(MWContext *context, NET_StreamClass *stream)
{
    MOZ_FUNCTION_STUB;
    return -1;
}

#if 1

/*
 *---------------------------------------------------------------------------
 * From ns/modules/libimg/src/if.c
 *---------------------------------------------------------------------------
 */

/*
 *  determine what kind of image data we are dealing with
 */
#ifndef XP_MAC
int
IL_Type(const char *buf, int32 len)
{
    MOZ_FUNCTION_STUB;
    return 0; /* IL_NOTFOUND */
}
#endif /* XP_MAC */ 

/*
 *---------------------------------------------------------------------------
 * From ns/modules/libimg/src/ilclient.c
 *---------------------------------------------------------------------------
 */

/*
 * Create an HTML stream and generate HTML describing
 * the image cache.  Use "about:memory-cache" URL to acess.
 */
static int 
IL_DisplayMemCacheInfoAsHTML(FO_Present_Types format_out, URL_Struct *urls,
                             OPAQUE_CONTEXT *cx)
{
    MOZ_FUNCTION_STUB;
    return 0;
}


#ifndef XP_MAC
char *
IL_HTMLImageInfo(char *url_address)
{
    MOZ_FUNCTION_STUB;
    return NULL;
}
#endif /* XP_MAC */ 


/* Set limit on approximate size, in bytes, of all pixmap storage used
   by the imagelib.  */
#ifndef XP_MAC
void
IL_SetCacheSize(uint32 new_size)
{
    MOZ_FUNCTION_STUB;
}
#endif /* XP_MAC */ 



/*
 *---------------------------------------------------------------------------
 * From ns/modules/libimg/src/external.c
 *---------------------------------------------------------------------------
 */
static NET_StreamClass *
IL_ViewStream(FO_Present_Types format_out, void *newshack, URL_Struct *urls,
              OPAQUE_CONTEXT *cx)
{
    MOZ_FUNCTION_STUB;
    return NULL;
}

#endif

/*
 *---------------------------------------------------------------------------
 * From ns/lib/libmisc/glhist.c
 *---------------------------------------------------------------------------
 */

PUBLIC void
NET_RegisterEnableUrlMatchCallback(void)
{
    MOZ_FUNCTION_STUB;
}


/* start global history tracking
 */
PUBLIC void
GH_InitGlobalHistory(void)
{
    MOZ_FUNCTION_STUB;
}


/* save the global history to a file while leaving the object in memory
 */
PUBLIC void
GH_SaveGlobalHistory(void)
{
    MOZ_FUNCTION_STUB;
}



/* create an HTML stream and push a bunch of HTML about
 * the global history
 */
MODULE_PRIVATE int
NET_DisplayGlobalHistoryInfoAsHTML(MWContext *context,
                                   URL_Struct *URL_s,
                                   int format_out)
{
    MOZ_FUNCTION_STUB;
    return MK_UNABLE_TO_CONVERT;
}



/*
 *---------------------------------------------------------------------------
 * From ns/lib/libmisc/shist.c
 *---------------------------------------------------------------------------
 */
PUBLIC int
SHIST_GetIndex(History * hist, History_entry * entry)
{
    MOZ_FUNCTION_STUB;
    return -1;
}


/*
 *---------------------------------------------------------------------------
 * From ns/lib/plugin/npglue.c
 *---------------------------------------------------------------------------
 */

XP_Bool
NPL_HandleURL(MWContext *cx, FO_Present_Types iFormatOut, URL_Struct *pURL, Net_GetUrlExitFunc *pExitFunc)
{
    MOZ_FUNCTION_STUB;
    return FALSE;
}


/*
 *---------------------------------------------------------------------------
 * From ns/lib/libmocha/lm_taint.c
 *---------------------------------------------------------------------------
 */

const char *
LM_SkipWysiwygURLPrefix(const char *url_string)
{
    MOZ_FUNCTION_STUB;
    return NULL;
}



/*
 *---------------------------------------------------------------------------
 * From ns/lib/htmldlgs/htmldlgs.c
 *---------------------------------------------------------------------------
 */

void
XP_HandleHTMLPanel(URL_Struct *url)
{
    MOZ_FUNCTION_STUB;
}


void
XP_HandleHTMLDialog(URL_Struct *url)
{
    MOZ_FUNCTION_STUB;
}


/*
 *---------------------------------------------------------------------------
 * From ns/nav-java/netscape/net/netStubs.c
 *---------------------------------------------------------------------------
 */


/*
 *---------------------------------------------------------------------------
 * From ns/lib/libmisc/glhist.c
 *---------------------------------------------------------------------------
 */

PUBLIC void
GH_UpdateGlobalHistory(URL_Struct * URL_s)
{
    MOZ_FUNCTION_STUB;
}


/* clear the global history list
 */
PUBLIC void
GH_ClearGlobalHistory(void)
{
    MOZ_FUNCTION_STUB;
}


/*
 *---------------------------------------------------------------------------
 * From ns/lib/libmisc/shist.c
 *---------------------------------------------------------------------------
 */


PUBLIC History_entry *
SHIST_GetCurrent(History * hist)
{
    MOZ_FUNCTION_STUB;
    return NULL;
}

#ifdef NS_NET_FILE
// XXXXXXXXXXXXXXXXXXXXXXXXX NS_NET_FILE BEGIN XXXXXXXXXXXXXXXXXXXXXXXXX 

// Begin nsNetFile versions of xp_file routines. These stubs utilize the new
// nsINetFile interface to do file i/o type things.


// Utility routine to remove a transator object from the array and free it.
nsresult deleteTrans(nsFile *nsFp) {
    xpNSFile *trans = nsnull;

    if (!nsFp)
        return NS_OK;

    for (PRInt32 i = switchBack.Count(); i > 0; i--) {
        trans = (xpNSFile*)switchBack.ElementAt(i-1);
        if (trans && trans->nsFp == nsFp) {
            switchBack.RemoveElement(trans);
            if (trans->xpFp) {
                PR_Free(trans->xpFp);
                trans->xpFp = nsnull;
            }
            PR_Free(trans);
            return NS_OK;
        }
    }
    return NS_ERROR_FAILURE;

}

// Utility routine to convert an xpFile pointer to a ns file pointer.
nsFile * XpToNsFp(XP_File xpFp) {
    nsFile *nsFp = nsnull;
    xpNSFile *trans = nsnull;

    if (!xpFp)
        return nsnull;
    for (PRInt32 i = switchBack.Count(); i > 0; i--) {
        trans = (xpNSFile*)switchBack.ElementAt(i-1);
        if (trans && trans->xpFp == xpFp) {
            nsFp = trans->nsFp;
            break;
        }
    }
    return nsFp;
}

/*
//
// Open a file with the given name
// If a special file type is provided we might need to get the name
//  out of the preferences list
//
*/

/* List of old xpFile enums we need to support 
 * Don't need cache stuff as new cache module will do it's own disk i/o
xpCacheFAT
xpCache
xpSARCache
xpExtCacheIndex

xpProxyConfig
xpURL
xpJSConfig
xpTemporary
xpJSCookieFilters
xpFileToPost
xpMimeTypes
xpHTTPCookie
xpHTTPCookiePermission
xpHTTPSingleSignon*/

// Caller is repsonsible for freeing string.

PRIVATE
char *xpFileTypeToName(XP_FileType type) {
    char *name = nsnull;
    switch (type) {
        case (xpCache):
            return PL_strdup(CACHE_DIR_TOK);
            break;
        case (xpCacheFAT):
            return PL_strdup(CACHE_DIR_TOK CACHE_DB_F_TOK);
            break;
        case (xpProxyConfig):
            break;
        case (xpURL):
            break;
        case (xpJSConfig):
            break;
        case (xpTemporary):
            break;
        case (xpJSCookieFilters):
            break;
        case (xpFileToPost):
            break;
        case (xpMimeTypes):
            break;
        case (xpHTTPCookie):
            return PL_strdup("%USER%%COOKIE_F%");
        case (xpHTTPCookiePermission):
            return PL_strdup("%USER%%COOKIE_PERMISSION_F%");
#ifdef SingleSignon
        case (xpHTTPSingleSignon):
            return PL_strdup("%USER%%SIGNON_F%");
#endif

        default:
            break;
    }
    return nsnull;
}

/*
// The caller is responsible for XP_FREE()ing the return string
*/
PUBLIC char *
WH_FileName (const char *NetName, XP_FileType type)
{
    char *path = nsnull, *aName = nsnull;
    nsresult rv;

    if (!NetName || !*NetName) {
        aName = xpFileTypeToName(type);
        if (!aName) {
            return NULL;
        }
    }

    if (!fileMgr) {
        if (NS_NewINetFile(&fileMgr, nsnull) != NS_OK) {
            return NULL;
        }
    }

    rv = fileMgr->GetFilePath( (aName ? aName : NetName), &path);
    PR_FREEIF(aName);
    if (rv != NS_OK)
        return NULL;

    return path;
}

char *
WH_TempName(XP_FileType type, const char * prefix)
{
    char *path = nsnull, *aName = nsnull;
    nsresult rv;

    aName = xpFileTypeToName(type);
    if (!aName) {
        return NULL;
    }


    if (!fileMgr) {
        if (NS_NewINetFile(&fileMgr, nsnull) != NS_OK) {
            return NULL;
        }
    }

    rv = fileMgr->GetCacheFileName(aName, &path);
    PR_FREEIF(aName);
    if (rv != NS_OK)
        return NULL;

    return path;
}

PUBLIC int
NET_I_XP_Fileno(XP_File fp) {
    nsFile *nsFp = XpToNsFp(fp); // nsnull ok.

    NS_PRECONDITION( (nsFp != nsnull), "Null pointer.");

    return (int) nsFp->fd;
}

PUBLIC int
NET_I_XP_FileSeek(XP_File fp, long offset, int origin) {
    nsFile *nsFp = XpToNsFp(fp); // nsnull ok.
    PRSeekWhence sw;

    NS_PRECONDITION( (nsFp != nsnull), "Null pointer.");

    /* Need to confirm the origin to PRSeekWhence mapping */

    switch (origin) {
        case (0):
            sw = PR_SEEK_SET;
            break;
        case (1):
            sw = PR_SEEK_CUR;
            break;
        case (2):
            sw = PR_SEEK_END;
            break;
        default:
            sw = PR_SEEK_SET;
    }

    return PR_Seek(nsFp->fd, offset, sw);
}

// Initialization routine for nsNetFile. This registers all the directories
// and files in the nsNetFile instance. Use the form:
// token = %token%
// value = opaque string
//
// Directories and files are platform specific.
PUBLIC PRBool
NET_InitFilesAndDirs(void) {
    PRFileInfo dir;
    PRStatus status;
    char tmpBuf[_MAX_PATH];
#ifdef XP_PC
    char *mDirDel = "\\";
#elif XP_MAC
    char *mDirDel = ":";
#else
    char *mDirDel = "/";
#endif
    
    // Setup directories, step 1.
    USER_DIR = _getcwd(USER_DIR, _MAX_PATH);
    CACHE_DIR = _getcwd(CACHE_DIR, _MAX_PATH);
    DEF_DIR = _getcwd(DEF_DIR, _MAX_PATH);

    // setup the cache dir.
    PL_strcpy(tmpBuf, CACHE_DIR);
    sprintf(CACHE_DIR,"%s%s%s%s", tmpBuf, mDirDel, "cache", mDirDel);
    status = PR_GetFileInfo(CACHE_DIR, &dir);
    if (status == PR_FAILURE) {
        // Create the dir.
        status = PR_MkDir(CACHE_DIR, 0600);
        if (status != PR_SUCCESS) {
            ; // bummer!
        }
    }

    if (!fileMgr) {
        if (NS_NewINetFile(&fileMgr, nsnull) != NS_OK) {
            return FALSE;
        }
    }

    // Setup directories.
    fileMgr->SetDirectory(USER_DIR_TOK, USER_DIR);
    fileMgr->SetDirectory(CACHE_DIR_TOK, CACHE_DIR);
    fileMgr->SetDirectory(DEF_DIR_TOK, DEF_DIR);

    // Setup files.
    fileMgr->SetFileAssoc(COOKIE_FILE_TOK, COOKIE_FILE, USER_DIR_TOK);
    fileMgr->SetFileAssoc(COOKIE_PERMISSION_FILE_TOK, COOKIE_PERMISSION_FILE, USER_DIR_TOK);
#ifdef SingleSignon
    fileMgr->SetFileAssoc(SIGNON_FILE_TOK, SIGNON_FILE, USER_DIR_TOK);
#endif
    fileMgr->SetFileAssoc(CACHE_DB_F_TOK, CACHE_DB_FILE, CACHE_DIR_TOK);
    return TRUE;
}

PUBLIC XP_File 
NET_I_XP_FileOpen(const char * name, XP_FileType type, const XP_FilePerm perm)
{
    XP_File xpFp;
    xpNSFile *trans = (xpNSFile*)PR_Malloc(sizeof(xpNSFile));
    nsFile *nsFp = nsnull;
    nsresult rv;
    nsFileMode mode;
    char *aName = nsnull;

    if (!fileMgr) {
        if (NS_NewINetFile(&fileMgr, nsnull) != NS_OK) {
            return NULL;
        }
    }

    // Just get some random address.
    xpFp = (XP_File) PR_Malloc(1);

    trans->xpFp= xpFp;

    if ((!PL_strcasecmp(perm,XP_FILE_READ)) ||
        (!PL_strcasecmp(perm,XP_FILE_READ_BIN))) {
        mode = nsRead;
    } else if (!PL_strcasecmp(perm,XP_FILE_WRITE)) {
        mode = nsOverWrite;
    } else if (!PL_strcasecmp(perm,XP_FILE_WRITE_BIN)) {
        mode = nsOverWrite;
    } else {
        mode = nsReadWrite;
    }

    /* call OpenFile with nsNetFile syntax if necesary. */
    if ( (!name || !*name) 
         || type == xpCache ) {
        char *tmpName = xpFileTypeToName(type);
        nsString newName = tmpName;
        PR_FREEIF(tmpName)
        if (newName.Length() < 1) {
            PR_Free(trans);
            PR_Free(xpFp);
            return NULL;
        }
        newName.Append(name);
        aName = newName.ToNewCString();
    }

    rv = fileMgr->OpenFile( (aName ? aName : name), mode, &nsFp);
    if (aName)
        delete[] aName;
    if (NS_OK != rv) {
        return NULL;
    }

    trans->nsFp = nsFp;

    switchBack.AppendElement(trans);

    return xpFp;
}

PUBLIC char *
NET_I_XP_FileReadLine(char *outBuf, int outBufLen, XP_File fp) {
    nsFile *nsFp = XpToNsFp(fp); // nsnull ok.
    PRInt32 readBytes;
    nsresult rv;

    NS_PRECONDITION( (nsFp != nsnull), "Null pointer");

    if (!nsFp)
        return NULL;

    if (!fileMgr)
        return NULL;

    rv = fileMgr->FileReadLine( nsFp, &outBuf, &outBufLen, &readBytes);
    if (NS_OK != rv) {
        return NULL;
    }

    return outBuf;
}

PUBLIC int
NET_I_XP_FileRead(char *outBuf, int outBufLen, XP_File fp) {
    nsFile *nsFp = XpToNsFp(fp); // nsnull ok.
    PRInt32 readBytes;
    nsresult rv;

    NS_PRECONDITION( (nsFp != nsnull), "Null pointer");

    if (!nsFp)
        return -1;

    if (!fileMgr)
        return -1;

    rv = fileMgr->FileRead( nsFp, &outBuf, &outBufLen, &readBytes);
    if (NS_OK != rv) {
        return -1;
    }

    return (int) readBytes;
}

PUBLIC int
NET_I_XP_FileWrite(const char *buf, int bufLen, XP_File fp) {
    nsFile *nsFp = XpToNsFp(fp); // nsnull ok.
    PRInt32 wroteBytes;
    PRInt32 len;
    nsresult rv;

    if (bufLen < 0)
        len = PL_strlen(buf);
    else
        len = bufLen;

    if (!nsFp)
        return -1;

    if (!fileMgr)
        return NULL;

    rv = fileMgr->FileWrite(nsFp, buf, &len, &wroteBytes);;
    if (rv != NS_OK)
        return NULL;

    return (int) wroteBytes;
}

PUBLIC int
NET_I_XP_FileClose(XP_File fp) {
    nsFile *nsFp = XpToNsFp(fp); // nsnull ok.
    nsresult rv;

    NS_PRECONDITION( (nsFp != nsnull), "Null pointer");

    if (!fileMgr) {
        if (NS_NewINetFile(&fileMgr, nsnull) != NS_OK) {
            return 0;
        }
    }

    rv = fileMgr->CloseFile(nsFp);
    if (rv != NS_OK)
        return 0;

    deleteTrans(nsFp);
    fp = nsnull;

    return 1;
}


/*
//
// Return 0 on success, -1 on failure.  
//
*/
PUBLIC int 
NET_I_XP_FileRemove(const char * name, XP_FileType type)
{
    char *path = nsnull, *aName = nsnull;
    nsresult rv;

    if ( (!name || !*name) 
         || type == xpCache ) {
        char *tmpName = xpFileTypeToName(type);
        nsString newName = tmpName;
        PR_FREEIF(tmpName);
        if (newName.Length() < 1) {
            return NULL;
        }
        newName.Append(name);
        aName = newName.ToNewCString();
    }

    if (!fileMgr) {
        if (NS_NewINetFile(&fileMgr, nsnull) != NS_OK) {
            return NULL;
        }
    }

    rv = fileMgr->FileRemove((aName ? aName : name));
    if (aName)
        delete[] aName;
    if (rv != NS_OK)
        return -1;

    return 0;
}

PUBLIC int 
NET_I_XP_Stat(const char * name, XP_StatStruct * info, XP_FileType type)
{
    int result = -1;
    PRFileInfo fileInfo;
    PRStatus status;
    char *newName, *tmpName;
    nsString ourName;
    char *path;
    nsresult rv;

    switch (type) {
        case xpCache:
            tmpName = xpFileTypeToName(type);
            if (!tmpName)
                return NULL;
            ourName = tmpName;
            PR_FREEIF(tmpName);
            ourName.Append(name);
            newName = ourName.ToNewCString();

            if (!fileMgr) {
                if (NS_NewINetFile(&fileMgr, nsnull) != NS_OK) {
                    delete[] newName;
                    return NULL;
                }
            }

            rv = fileMgr->GetFilePath(newName, &path);
            if (rv != NS_OK) {
                delete[] newName;
                return -1;
            }
            status = PR_GetFileInfo(path, &fileInfo);
            PR_Free(path);
            delete[] newName;
            if (status == PR_SUCCESS) {
                // transfer the pr stat info over to the xp stat object.
                // right now just moving over the size.
                info->st_size = fileInfo.size;
                return 0;              
            }
            break;
        case xpURL:
        case xpFileToPost: {
            newName = WH_FileName(name, type);
    	
            if (!newName) return -1;
            result = _stat( newName, info );
            PR_Free(newName);
            break;
        }
        default:
            break;
    }
    return result;
}

PUBLIC XP_Dir 
NET_I_XP_OpenDir(const char * name, XP_FileType type)
{
    MOZ_FUNCTION_STUB;
    return NULL;
}

PUBLIC void 
NET_I_XP_CloseDir(XP_Dir dir)
{
    MOZ_FUNCTION_STUB;
}
/*
//
// Close the directory
//
*/

PUBLIC XP_DirEntryStruct * 
NET_I_XP_ReadDir(XP_Dir dir)
{                                         
    MOZ_FUNCTION_STUB;
    return NULL;
}
// XXXXXXXXXXXXXXXXXXXXXXXXX NS_NET_FILE END XXXXXXXXXXXXXXXXXXXXXXXXX 
#endif /* NS_NET_FILE */

#ifdef XP_MAC
char *
WH_TempName(XP_FileType type, const char * prefix)
{
    MOZ_FUNCTION_STUB;
    return NULL;
}
#endif

// Begin vanilla versions of xp_file routines. These are simply stubs.
#ifdef XP_PC

#ifndef NS_NET_FILE

char *
WH_TempName(XP_FileType type, const char * prefix)
{
    MOZ_FUNCTION_STUB;
    return NULL;
}

/*
// The caller is responsible for XP_FREE()ing the return string
*/
PUBLIC char *
WH_FileName (const char *NetName, XP_FileType type)
{
    MOZ_FUNCTION_STUB;

    if (type == xpHTTPCookie) {
#ifdef XP_PC
        return PL_strdup("cookies.txt");
#else
        return PL_strdup("cookies");
#endif
    } else if (type == xpHTTPCookiePermission) {
#ifdef XP_PC
        return PL_strdup("cookperm.txt");
#else
        return PL_strdup("cookperm");
#endif
    } else if (type == xpHTTPSingleSignon) {
#ifdef XP_PC
        return PL_strdup("signons.txt");
#else
        return PL_strdup("signons");
#endif
    } else if (type == xpCacheFAT) {
;//		sprintf(newName, "%s\\fat.db", (const char *)theApp.m_pCacheDir);
        
    } else if ((type == xpURL) || (type == xpFileToPost)) {
        /*
         * This is the body of XP_NetToDosFileName(...) which is implemented 
         * for Windows only in fegui.cpp
         */
        XP_Bool bChopSlash = FALSE;
        char *p, *newName;

        if(!NetName)
            return NULL;
        
        //  If the name is only '/' or begins '//' keep the
        //    whole name else strip the leading '/'

        if(NetName[0] == '/')
            bChopSlash = TRUE;

        // save just / as a path
        if(NetName[0] == '/' && NetName[1] == '\0')
            bChopSlash = FALSE;

        // spanky Win9X path name
        if(NetName[0] == '/' && NetName[1] == '/')
            bChopSlash = FALSE;

        if(bChopSlash)
            newName = PL_strdup(&(NetName[1]));
        else
            newName = PL_strdup(NetName);

        if(!newName)
            return NULL;

        if( newName[1] == '|' )
            newName[1] = ':';

        for(p = newName; *p; p++){
            if( *p == '/' )
                *p = '\\';
        }
        return(newName);
    }

    return NULL;
}
#endif // !NS_NET_FILE

PUBLIC XP_File 
XP_FileOpen(const char * name, XP_FileType type, const XP_FilePerm perm)
{
    MOZ_FUNCTION_STUB;
    switch (type) {
        case xpURL:
        case xpFileToPost:
        case xpHTTPCookie:
        case xpHTTPCookiePermission:
#ifdef SingleSignon
        case xpHTTPSingleSignon:
#endif
        {
            XP_File fp;
            char* newName = WH_FileName(name, type);

            if (!newName) return NULL;

        	fp = fopen(newName, (char *) perm);
            XP_FREE(newName);
            return fp;

        }
        default:
            break;
    }

    return NULL;
}

PUBLIC XP_Dir 
XP_OpenDir(const char * name, XP_FileType type)
{
    MOZ_FUNCTION_STUB;
    return NULL;
}

PUBLIC void 
XP_CloseDir(XP_Dir dir)
{
    MOZ_FUNCTION_STUB;
}

PUBLIC XP_DirEntryStruct * 
XP_ReadDir(XP_Dir dir)
{                                         
    MOZ_FUNCTION_STUB;
    return NULL;
}

PUBLIC int 
XP_FileRemove(const char * name, XP_FileType type)
{
    if (PR_Delete(name) == PR_SUCCESS)
        return 0;
    return -1;
}

/*
//
// Mimic unix stat call
// Return -1 on error
//
*/
PUBLIC int 
XP_Stat(const char * name, XP_StatStruct * info, XP_FileType type)
{
    int result = -1;
    MOZ_FUNCTION_STUB;

    switch (type) {
        case xpURL:
        case xpFileToPost: {
            char *newName = WH_FileName(name, type);
    	
            if (!newName) return -1;
            result = _stat( newName, info );
            XP_FREE(newName);
            break;
        }
        default:
            break;
    }
    return result;
}
#endif /* XP_PC */

PUBLIC void *
FE_AboutData (const char *which,
              char **data_ret, int32 *length_ret, char **content_type_ret)
{
    MOZ_FUNCTION_STUB;

    *data_ret = NULL;
    *length_ret = 0;
    *content_type_ret = NULL;

    return NULL;
}


PUBLIC void
FE_FreeAboutData (void * data, const char* which2)
{
    MOZ_FUNCTION_STUB;
}

/*
 *---------------------------------------------------------------------------
 * From ns/cmd/winfe/fenet.cpp
 *---------------------------------------------------------------------------
 */

int FE_AsyncDNSLookup(MWContext *context, char * host_port, PRHostEnt ** hoststruct_ptr_ptr, PRFileDesc *socket)
{
    MOZ_FUNCTION_STUB;
    return -1;
}

/*
// INTL_ResourceCharSet(void)
//
*/
char *INTL_ResourceCharSet(void)
{
    MOZ_FUNCTION_STUB;

    return NULL;
}


/*
 *---------------------------------------------------------------------------
 * From ns/cmd/winfe/cfe.cpp
 *---------------------------------------------------------------------------
 */

int32 FE_GetContextID(MWContext *pContext)
{
    MOZ_FUNCTION_STUB;
    return 0;
}


XP_Bool FE_IsNetcasterInstalled(void) 
{
    MOZ_FUNCTION_STUB;
    return FALSE;
}

/*
//  A Url has changed context.
//  We need to mark it in the new context if it has ncapi_data (which we use
//  to track such things under windows).
*/
void FE_UrlChangedContext(URL_Struct *pUrl, MWContext *pOldContext, MWContext *pNewContext)
{
    MOZ_FUNCTION_STUB;
}



/*
 *---------------------------------------------------------------------------
 * From ns/cmd/winfe/urlecho.cpp
 *---------------------------------------------------------------------------
 */
void FE_URLEcho(URL_Struct *pURL, int iStatus, MWContext *pContext)  {
/*
//  Purpose:  Echo the URL to all appropriately registered applications that are monitoring such URL traffic.
//  Arguments:  pURL  The URL which is being loaded.
//        iStatus The status of the load.
//        pContext  The context in which the load occurred.
//  Returns:  void
//  Comments: Well, sometimes there isn't a window in which the load occurred, what to do then?
//        Just don't report.
//  Revision History:
//    01-18-95  created GAB
//
*/
    MOZ_FUNCTION_STUB;
}



/*
 *---------------------------------------------------------------------------
 * From ns/cmd/winfe/edview2.cpp
 *---------------------------------------------------------------------------
 */
/*
// Note: This is used by Navigator's HTP UPLOAD as well as Composer's file saving
//  DON'T ASSUME ANY EDITOR FUNCTIONALITY!
// Dialog to give feedback and allow canceling, overwrite protection
//   when downloading remote files 
//
*/
void FE_SaveDialogCreate( MWContext *pMWContext, int iFileCount, ED_SaveDialogType saveType  )
{
    MOZ_FUNCTION_STUB;
}


void FE_SaveDialogSetFilename( MWContext *pMWContext, char *pFilename )
{
    MOZ_FUNCTION_STUB;
}


void FE_SaveDialogDestroy( MWContext *pMWContext, int status, char *pFileURL )
{
    MOZ_FUNCTION_STUB;
}



/*
 *---------------------------------------------------------------------------
 * From ns/cmd/winfe/compmapi.cpp
 *---------------------------------------------------------------------------
 */
void FE_AlternateCompose(
    char * from, char * reply_to, char * to, char * cc, char * bcc,
    char * fcc, char * newsgroups, char * followup_to,
    char * organization, char * subject, char * references,
    char * other_random_headers, char * priority,
    char * attachment, char * newspost_url, char * body)
{
    MOZ_FUNCTION_STUB;
}


/*
 *---------------------------------------------------------------------------
 * From ns/cmd/winfe/nethelp.cpp
 *---------------------------------------------------------------------------
 */
MWContext *FE_GetNetHelpContext()
{
    MOZ_FUNCTION_STUB;
    return NULL;
}

/*
// Called from mkhelp.c to get the standard location of the NetHelp folder as a URL
*/
char * FE_GetNetHelpDir()
{
    MOZ_FUNCTION_STUB;
    return NULL;
}


/*
 *---------------------------------------------------------------------------
 * From ns/cmd/winfe/femess.cpp
 *---------------------------------------------------------------------------
 */
int FE_GetURL(MWContext *pContext, URL_Struct *pUrl)
{
    MOZ_FUNCTION_STUB;

    return -1;
}



/*
 *---------------------------------------------------------------------------
 * From ns/cmd/winfe/fmabstra.cpp
 *---------------------------------------------------------------------------
 */
void FE_RaiseWindow(MWContext *pContext)
{
    MOZ_FUNCTION_STUB;
}




/*
 *---------------------------------------------------------------------------
 * From ns/cmd/winfe/fegrid.cpp
 *---------------------------------------------------------------------------
 */
/*
//  Create a new window.
//  If pChrome is NULL, do a FE_MakeBlankWindow....
//  pChrome specifies the attributes of a window.
//  If you use this call, Toolbar information will not be saved in the preferences.
*/
MWContext *FE_MakeNewWindow(MWContext *pOldContext, URL_Struct *pUrl, char *pContextName, Chrome *pChrome)
{
    MOZ_FUNCTION_STUB;

    return NULL;
}



/*
 *---------------------------------------------------------------------------
 * From ns/cmd/winfe/timer.cpp
 *---------------------------------------------------------------------------
 */
/* this function should register a function that will
 * be called after the specified interval of time has
 * elapsed.  This function should return an id 
 * that can be passed to FE_ClearTimeout to cancel 
 * the Timeout request.
 *
 * A) Timeouts never fail to trigger, and
 * B) Timeouts don't trigger *before* their nominal timestamp expires, and
 * C) Timeouts trigger in the same ordering as their timestamps
 *
 * After the function has been called it is unregistered
 * and will not be called again unless re-registered.
 *
 * func:    The function to be invoked upon expiration of
 *          the Timeout interval 
 * closure: Data to be passed as the only argument to "func"
 * msecs:   The number of milli-seconds in the interval
 */
PUBLIC void * 
FE_SetTimeout(TimeoutCallbackFunction func, void * closure, uint32 msecs)
{
    MOZ_FUNCTION_STUB;
    return NULL;
}

/*
 *---------------------------------------------------------------------------
 * From ns/lib/libmocha/et_moz.c
 *---------------------------------------------------------------------------
 */

JSBool
ET_PostMessageBox(MWContext* context, char* szMessage, JSBool bConfirm)
{
    MOZ_FUNCTION_STUB;
    fprintf(stdout, "%c%s  (y/n)?  ", '\007', szMessage); /* \007 is BELL */
    char c;
    for (;;) {
	c = getchar();
        if (tolower(c) == 'y') {
	    return JS_TRUE;
	}
        if (tolower(c) == 'n') {
	    return JS_FALSE;
	}
    }
}

JSBool
ET_PostCheckConfirmBox(MWContext* context,
	char* szMainMessage, char* szCheckMessage,
	char* szOKMessage, char* szCancelMessage,
	XP_Bool *bChecked)
{
    MOZ_FUNCTION_STUB;
    fprintf(stdout, "%c%s  (y/n)?  ", '\007', szMainMessage); /* \007 is BELL */
    char c;
    XP_Bool result;
    for (;;) {
	c = getchar();
        if (tolower(c) == 'y') {
	    result = JS_TRUE;
	    break;
	}
        if (tolower(c) == 'n') {
	    result = JS_FALSE;
	    break;
	}
    }
    fprintf(stdout, "%c%s  y/n?  ", '\007', szCheckMessage); /* \007 is BELL */
    for (;;) {
	c = getchar();
        if (tolower(c) == 'y') {
	    *bChecked = TRUE;
	    break;
	}
        if (tolower(c) == 'n') {
	    *bChecked = FALSE;
	    break;
	}
    }
    return result;
}


/*
 * Tell the backend about a new load event.
 */
void
ET_SendLoadEvent(MWContext * pContext, int32 type, ETVoidPtrFunc fnClosure,
                 NET_StreamClass *stream, int32 layer_id, Bool resize_reload)
{
    MOZ_FUNCTION_STUB;
}

/*
 *---------------------------------------------------------------------------
 * From ns/lib/libmocha/lm_doc.c
 *---------------------------------------------------------------------------
 */

NET_StreamClass *
LM_WysiwygCacheConverter(MWContext *context, URL_Struct *url_struct,
                         const char * wysiwyg_url, const char * base_href)
{
    MOZ_FUNCTION_STUB;
    return NULL;
}


/*
 *---------------------------------------------------------------------------
 * From ns/lib/libmocha/lm_win.c
 *---------------------------------------------------------------------------
 */
/*
 * Entry point for front-ends to notify JS code of help events.
 */
void
LM_SendOnHelp(MWContext *context)
{
    MOZ_FUNCTION_STUB;
}

/*
 *---------------------------------------------------------------------------
 * From ns/lib/xp/xplocale.c
 *---------------------------------------------------------------------------
 */

const char* INTL_ctime(MWContext* context, time_t *date)
{
  static char result[40];	

  MOZ_FUNCTION_STUB;
  *result = '\0';
  return result;
}


/*
 *---------------------------------------------------------------------------
 * From ns/lib/xp/xp_cntxt.c
 *---------------------------------------------------------------------------
 */

/*
 * Finds a context that should be loaded with the URL, given
 * a name and current (refering) context.
 *
 * If the context returned is not NULL, name is already assigned to context
 * structure. You should load the URL into this context.
 *
 * If you get back a NULL, you should create a new window
 *
 * Both context and current context can be null.
 * Once the grids are in, there should be some kind of a logic that searches
 * siblings first. 
 */

MWContext * XP_FindNamedContextInList(MWContext * context, char *name)
{
    MOZ_FUNCTION_STUB;
    return NULL;
}


MWContext*
XP_FindSomeContext()
{
    MOZ_FUNCTION_STUB;
    return NULL;
}

/*
 *---------------------------------------------------------------------------
 * From ns/lib/plugin/npglue.c
 *---------------------------------------------------------------------------
 */

/* 
 * This exit routine is used for all streams requested by the
 * plug-in: byterange request streams, NPN_GetURL streams, and 
 * NPN_PostURL streams.  NOTE: If the exit routine gets called
 * in the course of a context switch, we must NOT delete the
 * URL_Struct.  Example: FTP post with result from server
 * displayed in new window -- the exit routine will be called
 * when the upload completes, but before the new context to
 * display the result is created, since the display of the
 * results in the new context gets its own completion routine.
 */
void
NPL_URLExit(URL_Struct *urls, int status, MWContext *cx)
{
    MOZ_FUNCTION_STUB;
}



/*
 *---------------------------------------------------------------------------
 * From ns/lib/layout/layedit.c
 *---------------------------------------------------------------------------
 */

void LO_SetBaseURL( MWContext *context, char *pURL )
{
    MOZ_FUNCTION_STUB;
}

/*
 *---------------------------------------------------------------------------
 * From ns/lib/layout/laysel.c
 *---------------------------------------------------------------------------
 */
/*
    get first(last) if current element is NULL.
*/
Bool
LO_getNextTabableElement( MWContext *context, LO_TabFocusData *pCurrentFocus, int forward )
{
    MOZ_FUNCTION_STUB;
    return FALSE;
}

PUBLIC int
NET_ParseNetHelpURL(URL_Struct *URL_s)
{
	MOZ_FUNCTION_STUB;
/*	return MK_OUT_OF_MEMORY; */
	return -207;
}

#ifdef XP_UNIX
PUBLIC NET_StreamClass* NET_ExtViewerConverter ( FO_Present_Types format_out,
                                                 void       *data_obj,
                                                 URL_Struct *URL_s,
                                                 MWContext  *window_id)
{
	return NULL;
}
#endif

/*
 * From xp_str.c
 */

/*	Allocate a new copy of a block of binary data, and returns it
 */
PUBLIC char * 
NET_BACopy (char **destination, const char *source, size_t length)
{
	if(*destination)
	  {
	    XP_FREE(*destination);
		*destination = 0;
	  }

    if (! source)
	  {
        *destination = NULL;
	  }
    else 
	  {
        *destination = (char *) PR_Malloc (length);
        if (*destination == NULL) 
	        return(NULL);
        memcpy(*destination, source, length);
      }
    return *destination;
}

/*	binary block Allocate and Concatenate
 *
 *   destination_length  is the length of the existing block
 *   source_length   is the length of the block being added to the 
 *   destination block
 */
PUBLIC char * 
NET_BACat (char **destination, 
		   size_t destination_length, 
		   const char *source, 
		   size_t source_length)
{
    if (source) 
	  {
        if (*destination) 
	      {
      	    *destination = (char *) PR_Realloc (*destination, destination_length + source_length);
            if (*destination == NULL) 
	          return(NULL);

            memmove (*destination + destination_length, source, source_length);

          } 
		else 
		  {
            *destination = (char *) PR_Malloc (source_length);
            if (*destination == NULL) 
	          return(NULL);

            memcpy(*destination, source, source_length);
          }
    }

  return *destination;
}

/*	Very similar to strdup except it free's too
 */
PUBLIC char * 
NET_SACopy (char **destination, const char *source)
{
	if(*destination)
	  {
	    XP_FREE(*destination);
		*destination = 0;
	  }
    if (! source)
	  {
        *destination = NULL;
	  }
    else 
	  {
        *destination = (char *) PR_Malloc (PL_strlen(source) + 1);
        if (*destination == NULL) 
 	        return(NULL);

        PL_strcpy (*destination, source);
      }
    return *destination;
}

/*  Again like strdup but it concatinates and free's and uses Realloc
*/
PUBLIC char *
NET_SACat (char **destination, const char *source)
{
    if (source && *source)
      {
        if (*destination)
          {
            int length = PL_strlen (*destination);
            *destination = (char *) PR_Realloc (*destination, length + PL_strlen(source) + 1);
            if (*destination == NULL)
            return(NULL);

            PL_strcpy (*destination + length, source);
          }
        else
          {
            *destination = (char *) PR_Malloc (PL_strlen(source) + 1);
            if (*destination == NULL)
                return(NULL);

             PL_strcpy (*destination, source);
          }
      }
    return *destination;
}

/* remove front and back white space
 * modifies the original string
 */
PUBLIC char *
XP_StripLine (char *string)
{
    char * ptr;

	/* remove leading blanks */
    while(*string=='\t' || *string==' ' || *string=='\r' || *string=='\n')
		string++;    

    for(ptr=string; *ptr; ptr++)
		;   /* NULL BODY; Find end of string */

	/* remove trailing blanks */
    for(ptr--; ptr >= string; ptr--) 
	  {
        if(*ptr=='\t' || *ptr==' ' || *ptr=='\r' || *ptr=='\n') 
			*ptr = '\0'; 
        else 
			break;
	  }

    return string;
}

/************************************************************************/

char *XP_AppendStr(char *in, const char *append)
{
    int alen, inlen;

    alen = PL_strlen(append);
    if (in) {
		inlen = PL_strlen(in);
		in = (char*) PR_Realloc(in,inlen+alen+1);
		if (in) {
			memcpy(in+inlen, append, alen+1);
		}
    } else {
		in = (char*) PR_Malloc(alen+1);
		if (in) {
			memcpy(in, append, alen+1);
		}
    }
    return in;
}

void RDF_AddCookieResource(char* name, char* path, char* host, char* expires)
{
	MOZ_FUNCTION_STUB;    
}


} /* end of extern "C" */
