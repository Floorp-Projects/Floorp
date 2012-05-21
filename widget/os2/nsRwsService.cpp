/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//------------------------------------------------------------------------

#include "nsIFile.h"
#include "mozilla/ModuleUtils.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsLiteralString.h"
#include "nsReadableUtils.h"
#include "nsIStringBundle.h"
#include "mozilla/Services.h"

#define INCL_WIN
#define INCL_DOS
#include <os2.h>

// nsRwsService must be included _after_ os2.h
#include "nsRwsService.h"
#include "rwserr.h"
#include "nsOS2Uni.h"

#include "prenv.h"
#include <stdio.h>

//------------------------------------------------------------------------
//  debug macros
//------------------------------------------------------------------------

#ifdef DEBUG
  #define RWS_DEBUG
#endif

// use this to force debug msgs in non-debug builds
//#ifndef RWS_DEBUG
//  #define RWS_DEBUG
//#endif

#ifdef RWS_DEBUG
  #define ERRMSG(x,y)       { printf(y " failed - rc= %d\n", (int)x); }

  #define ERRBREAK(x,y)     if (x) { ERRMSG(x,y); break; }

  #define ERRPRINTF(x,y)    { printf(y "\n", x); }
#else
  #define ERRMSG(x,y)       ;

  #define ERRBREAK(x,y)     if (x) break;

  #define ERRPRINTF(x,y)    ;
#endif

//------------------------------------------------------------------------
//  function prototypes
//------------------------------------------------------------------------

static nsresult IsDescendedFrom(PRUint32 wpsFilePtr, const char *pszClassname);
static nsresult CreateFileForExtension(const char *aFileExt, nsACString& aPath);
static nsresult DeleteFileForExtension(const char *aPath);
static void     AssignNLSString(const PRUnichar *aKey, nsAString& _retval);
static nsresult AssignTitleString(const char *aTitle, nsAString& result);

//------------------------------------------------------------------------
//  module-level variables - not declared as members of nsRws
//------------------------------------------------------------------------

static nsRwsService *sRwsInstance = 0;   // our singleton instance
static bool          sInit = FALSE;      // have we tried to load RWS

// RWS administrative functions
static ULONG (* _System sRwsClientInit)(BOOL);
static ULONG (* _System sRwsGetTimeout)(PULONG, PULONG);
static ULONG (* _System sRwsSetTimeout)(ULONG);
static ULONG (* _System sRwsClientTerminate)(void);

// RWS task-oriented functions
static ULONG (* _System sRwsCall)(PRWSHDR*, ULONG, ULONG, ULONG, ULONG, ULONG, ...);
static ULONG (* _System sRwsCallIndirect)(PRWSBLD);
static ULONG (* _System sRwsFreeMem)(PRWSHDR);
static ULONG (* _System sRwsGetResult)(PRWSHDR, ULONG, PULONG);
static ULONG (* _System sRwsGetArgPtr)(PRWSHDR, ULONG, PRWSDSC*);

//------------------------------------------------------------------------
//  ExtCache - caches the default icons & handlers for file extensions
//------------------------------------------------------------------------

typedef struct _ExtInfo
{
  char       ext[8];
  PRUint32   icon;
  PRUint32   mini;
  PRUint32   handler;
  PRUnichar *title;
} ExtInfo;

#define kGrowBy         8
#define kMutexTimeout   500

class ExtCache
{
public:
  ExtCache();
  ~ExtCache();

  nsresult GetIcon(const char *aExt, bool aNeedMini, PRUint32 *oIcon);
  nsresult SetIcon(const char *aExt, bool aIsMini, PRUint32 aIcon);
  nsresult GetHandler(const char *aExt, PRUint32 *oHandle, nsAString& oTitle);
  nsresult SetHandler(const char *aExt, PRUint32 aHandle, nsAString& aTitle);
  void     EmptyCache();

protected:
  ExtInfo *FindExtension(const char *aExt, bool aSet = false);

  PRUint32 mPid;
  PRUint32 mMutex;
  PRUint32 mCount;
  PRUint32 mSize;
  ExtInfo *mExtInfo;
};

//------------------------------------------------------------------------
//  nsIRwsService implementation
//------------------------------------------------------------------------

NS_IMPL_ISUPPORTS2(nsRwsService, nsIRwsService, nsIObserver)

nsRwsService::nsRwsService()
{
  mExtCache = new ExtCache();
  if (!mExtCache)
    ERRPRINTF("", "nsRwsService - unable to allocate mExtArray%s");
}

nsRwsService::~nsRwsService()
{
}

//------------------------------------------------------------------------

// provides the default icon associated with a given extension;  if the
// icon isn't in the cache, it creates a temp file with that extension,
// retrieves its icon, deletes the temp file, then caches the icon

NS_IMETHODIMP
nsRwsService::IconFromExtension(const char *aExt, bool aNeedMini,
                                PRUint32 *_retval)
{
  if (!aExt || !*aExt || !_retval)
    return NS_ERROR_INVALID_ARG;

  nsresult rv = mExtCache->GetIcon(aExt, aNeedMini, _retval);
  if (NS_SUCCEEDED(rv))
    return rv;

  nsCAutoString path;
  rv = CreateFileForExtension(aExt, path);
  if (NS_SUCCEEDED(rv)) {
    rv = IconFromPath(path.get(), false, aNeedMini, _retval);
    DeleteFileForExtension(path.get());
    if (NS_SUCCEEDED(rv))
      mExtCache->SetIcon(aExt, aNeedMini, *_retval);
  }

  return rv;
}

//------------------------------------------------------------------------

// retrieves an object's icon using its fully-qualified path;  aAbstract
// indicates whether this is a Filesystem object or an Abstract or
// Transient object;  locating non-file objects is fairly expensive

NS_IMETHODIMP
nsRwsService::IconFromPath(const char *aPath, bool aAbstract,
                           bool aNeedMini, PRUint32 *_retval)
{
  if (!aPath || !*aPath || !_retval)
    return NS_ERROR_INVALID_ARG;

  PRUint32  rwsType;

  if (aAbstract)
    rwsType = (aNeedMini ? RWSC_OFTITLE_OMINI : RWSC_OFTITLE_OICON);
  else
    rwsType = (aNeedMini ? RWSC_OPATH_OMINI : RWSC_OPATH_OICON);

  return RwsConvert(rwsType, (PRUint32)aPath, _retval);
}

//------------------------------------------------------------------------

// retrieve's an object's icon using its persistent handle

NS_IMETHODIMP
nsRwsService::IconFromHandle(PRUint32 aHandle, bool aNeedMini,
                             PRUint32 *_retval)
{
  if (!aHandle || !_retval)
    return NS_ERROR_INVALID_ARG;

  return RwsConvert( (aNeedMini ? RWSC_OHNDL_OMINI : RWSC_OHNDL_OICON),
                     aHandle, _retval);
}

//------------------------------------------------------------------------

// retrieve's an object's title using its persistent handle

NS_IMETHODIMP
nsRwsService::TitleFromHandle(PRUint32 aHandle, nsAString& _retval)
{
  if (!aHandle)
    return NS_ERROR_INVALID_ARG;

  return RwsConvert(RWSC_OHNDL_OTITLE, aHandle, _retval);
}

//------------------------------------------------------------------------

// provides the default handler associated with a given extension;  if the
// info isn't in the cache, it creates a temp file with that extension,
// retrieves the handler's title & possibly its object handle, deletes the
// temp file, then caches the info.

NS_IMETHODIMP
nsRwsService::HandlerFromExtension(const char *aExt, PRUint32 *aHandle,
                                   nsAString& _retval)
{
  if (!aExt || !*aExt || !aHandle)
    return NS_ERROR_INVALID_ARG;

  nsresult rv = mExtCache->GetHandler(aExt, aHandle, _retval);
  if (NS_SUCCEEDED(rv))
    return rv;

  nsCAutoString path;
  rv = CreateFileForExtension(aExt, path);
  if (NS_SUCCEEDED(rv)) {
    rv = HandlerFromPath(path.get(), aHandle, _retval);
    DeleteFileForExtension(path.get());
    if (NS_SUCCEEDED(rv))
      mExtCache->SetHandler(aExt, *aHandle, _retval);
  }

  return rv;
}

//------------------------------------------------------------------------

// identifies the default WPS handler for a given file;  if the handler
// is an associated program, returns its title & object handle;  if the
// handler is an integrated class viewer or player, it constructs a title
// and sets the object handle to zero

NS_IMETHODIMP
nsRwsService::HandlerFromPath(const char *aPath, PRUint32 *aHandle,
                              nsAString& _retval)
{
  if (!aPath || !*aPath || !aHandle)
    return NS_ERROR_INVALID_ARG;

  nsresult  rv = NS_ERROR_FAILURE;
  PRWSHDR   pHdr = 0;
  PRUint32  rc;

  _retval.Truncate();
  *aHandle = 0;

  // this dummy do{...}while(0) loop lets us bail out early
  // while ensuring pHdr gets freed
  do {
    // get the default view for this file
    rc = sRwsCall(&pHdr,
                  RWSP_MNAM, (ULONG)"wpQueryDefaultView",
                  RWSR_ASIS,  0, 1,
                  RWSI_OPATH, 0, (ULONG)aPath);
    ERRBREAK(rc, "wpQueryDefaultView")

    PRUint32 defView = sRwsGetResult(pHdr, 0, 0);
    if (defView == (PRUint32)-1)
      break;

    // if the default view is OPEN_SETTINGS ('2'),
    // change it to OPEN_RUNNING ('4')
    if (defView == 2)
      defView = 4;

    // to improve efficiency, retrieve & reuse the file's wpObject
    // ptr rather than repeatedly converting the name to an object
    PRUint32 wpsFilePtr = sRwsGetResult(pHdr, 1, 0);

    // free pHdr before the next call
    sRwsFreeMem(pHdr);
    pHdr = 0;

    // for default view values less than OPEN_USER, see if there
    // is a program or program object associated with this file
    if (defView < 0x6500) {
      rc = sRwsCall(&pHdr,
                    RWSP_MNAM, (ULONG)"wpQueryAssociatedProgram",
                    RWSR_OHNDL,  0, 6,
                    RWSI_ASIS,   0, wpsFilePtr,
                    RWSI_ASIS,   0, defView,
                    RWSI_PULONG, 0, 0,
                    RWSI_PBUF,  32, 0,
                    RWSI_ASIS,   0, 32,
                    RWSI_ASIS,   0, (ULONG)-1);

      // if there's no associated program, create dummy values
      // (if chosen, the WPS will open the file's Properties notebook)
      if (rc) {
        if (rc == RWSSRV_FUNCTIONFAILED) {
          *aHandle = 0;
          AssignNLSString(NS_LITERAL_STRING("wpsDefaultOS2").get(), _retval);
          rv = NS_OK;
        }
        else
          ERRMSG(rc, "wpQueryAssociatedProgram")
        break;
      }

      // get a ptr to the return value's data structure;  then get
      // both the object handle we requested & the raw WPS object ptr
      PRWSDSC pRtn;
      rc = sRwsGetArgPtr(pHdr, 0, &pRtn);
      ERRBREAK(rc, "GetArgPtr")
      *aHandle = *((PRUint32*)pRtn->pget);
      PRUint32 wpsPgmPtr = pRtn->value;

      // free pHdr before the next call
      sRwsFreeMem(pHdr);
      pHdr = 0;

      // convert the object to its title (using Rws' conversion
      // feature is more efficient than calling wpQueryTitle)
      rc = sRwsCall(&pHdr,
                    RWSP_CONV, 0,
                    RWSR_ASIS, 0, 1,
                    RWSC_OBJ_OTITLE, 0, wpsPgmPtr);
      ERRBREAK(rc, "convert pgm object to title")

      // retrieve the title, massage it & assign to _retval, then exit
      // N.B. no need to free pHdr here, it will be freed below
      char *pszTitle = (char*)sRwsGetResult(pHdr, 1, 0);
      if (pszTitle != (char*)-1)
        rv = AssignTitleString(pszTitle, _retval);

      break;
    }

    // the default view is provided by the file's WPS class;
    // in this case, *aHandle is 0

    // see if it's a known view that can be given a meaningful name
    switch (defView) {
      case 0xbc2b: {
        rv = IsDescendedFrom(wpsFilePtr, "MMImage");
        if (NS_SUCCEEDED(rv))
          AssignNLSString(NS_LITERAL_STRING("mmImageViewerOS2").get(), _retval);
        break;
      }

      case 0xbc0d:    // Audio editor
      case 0xbc21:    // Video editor
      case 0xbc17:    // Midi editor
      case 0xbbef:    // Play
      case 0xbbe5: {  // Player
        rv = IsDescendedFrom(wpsFilePtr, "MMAudio");
        if (NS_SUCCEEDED(rv)) {
          AssignNLSString(NS_LITERAL_STRING("mmAudioPlayerOS2").get(), _retval);
          break;
        }

        rv = IsDescendedFrom(wpsFilePtr, "MMVideo");
        if (NS_SUCCEEDED(rv)) {
          AssignNLSString(NS_LITERAL_STRING("mmVideoPlayerOS2").get(), _retval);
          break;
        }

        rv = IsDescendedFrom(wpsFilePtr, "MMMIDI");
        if (NS_SUCCEEDED(rv))
          AssignNLSString(NS_LITERAL_STRING("mmMidiPlayerOS2").get(), _retval);

        break;
      }

      case 0x7701: {
        rv = IsDescendedFrom(wpsFilePtr, "TSArcMgr");
        if (NS_SUCCEEDED(rv))
          AssignNLSString(NS_LITERAL_STRING("odZipFolderOS2").get(), _retval);
        break;
      }

      // this has to go last because TSEnhDataFile replaces
      // WPDataFile; if present, all data files are descended from it
      case 0xb742:
      case 0xa742: {
        rv = IsDescendedFrom(wpsFilePtr, "TSEnhDataFile");
        if (NS_SUCCEEDED(rv))
          AssignNLSString(NS_LITERAL_STRING("odTextViewOS2").get(), _retval);
        break;
      }
    } // end switch

    if (NS_SUCCEEDED(rv))
      break;

    // the name of this view is unknown, so create a generic name
    // (i.e. "Viewer for Class WPSomething")

    // use the file's object ptr to get the name of its class
    rc = sRwsCall(&pHdr,
                  RWSP_CONV, 0,
                  RWSR_ASIS, 0, 1,
                  RWSC_OBJ_CNAME, 0, wpsFilePtr);
    ERRBREAK(rc, "convert object to classname")

    char *pszTitle = (char*)sRwsGetResult(pHdr, 1, 0);
    if (pszTitle == (char*)-1)
      break;

    nsAutoChar16Buffer buffer;
    PRInt32 bufLength;
    rv = MultiByteToWideChar(0, pszTitle, strlen(pszTitle),
                             buffer, bufLength);
    if (NS_FAILED(rv))
      break;

    nsAutoString classViewer;
    AssignNLSString(NS_LITERAL_STRING("classViewerOS2").get(), classViewer);
    int pos = -1;
    if ((pos = classViewer.Find("%S")) > -1)
      classViewer.Replace(pos, 2, buffer.Elements());
    _retval.Assign(classViewer);
    rv = NS_OK;
  } while (0);

  // free the pHdr allocated by the final call
  sRwsFreeMem(pHdr);
  return rv;
}

//------------------------------------------------------------------------

// retrieves an object's menu using its fully-qualified path;  aAbstract
// indicates whether this is a Filesystem object or an Abstract or
// Transient object;  locating non-file objects is fairly expensive

NS_IMETHODIMP
nsRwsService::MenuFromPath(const char *aPath, bool aAbstract)
{
  if (!aPath || !*aPath)
    return NS_ERROR_INVALID_ARG;

  nsresult  rv = NS_ERROR_FAILURE;
  PRWSHDR   pHdr = 0;
  PRUint32  type = (aAbstract ? RWSI_OFTITLE : RWSI_OPATH);
  PRUint32  rc;
  POINTL    ptl;
  HWND      hTgt = 0;

  // try to identify the window where the click occurred
  if (WinQueryMsgPos(0, &ptl)) {
    hTgt = WinQueryFocus(HWND_DESKTOP);
    if (hTgt)
      WinMapWindowPoints(HWND_DESKTOP, hTgt, &ptl, 1);
  }

  // if we have the window & coordinates, use them;  otherwise,
  // let RWS position the menu at the current pointer position
  // (specifying the window ensures the focus will return to it)
  if (hTgt)
      rc = sRwsCall(&pHdr,
                    RWSP_CMD,  RWSCMD_POPUPMENU,
                    RWSR_ASIS, 0, 3,
                    type,      0, (ULONG)aPath,
                    RWSI_ASIS, 0, hTgt,
                    RWSI_PBUF, sizeof(POINTL), (ULONG)&ptl);
  else
      rc = sRwsCall(&pHdr,
                    RWSP_CMD,  RWSCMD_POPUPMENU,
                    RWSR_ASIS, 0, 3,
                    type,      0, (ULONG)aPath,
                    RWSI_ASIS, 0, 0,
                    RWSI_ASIS, 0, 0);

  if (rc)
    ERRMSG(rc, "RWSCMD_POPUPMENU")
  else
    rv = NS_OK;

  // free pHdr
  sRwsFreeMem(pHdr);
  return rv;
}

//------------------------------------------------------------------------

// causes the WPS to open a file using the program specified by AppPath;
// this is identical to dropping the file on the program's WPS icon

NS_IMETHODIMP
nsRwsService::OpenWithAppHandle(const char *aFilePath, PRUint32 aAppHandle)
{
  if (!aFilePath || !*aFilePath || !aAppHandle)
    return NS_ERROR_INVALID_ARG;

  nsresult  rv = NS_ERROR_FAILURE;
  PRWSHDR   pHdr = 0;
  PRUint32  rc;

  rc = sRwsCall(&pHdr,
                RWSP_CMD,   RWSCMD_OPENUSING,
                RWSR_ASIS,  0, 2,
                RWSI_OPATH, 0, aFilePath,
                RWSI_OHNDL, 0, aAppHandle);
  if (rc)
    ERRMSG(rc, "RWSCMD_OPENUSING")
  else
    rv = NS_OK;

  sRwsFreeMem(pHdr);
  return rv;
}

//------------------------------------------------------------------------

// causes the WPS to open a file using the program specified by AppPath;
// this is identical to dropping the file on the program's WPS icon

NS_IMETHODIMP
nsRwsService::OpenWithAppPath(const char *aFilePath, const char *aAppPath)
{
  if (!aFilePath || !*aFilePath || !aAppPath || !*aAppPath)
    return NS_ERROR_INVALID_ARG;

  nsresult  rv = NS_ERROR_FAILURE;
  PRWSHDR   pHdr = 0;
  PRUint32  rc;

  rc = sRwsCall(&pHdr,
                RWSP_CMD,   RWSCMD_OPENUSING,
                RWSR_ASIS,  0, 2,
                RWSI_OPATH, 0, aFilePath,
                RWSI_OPATH, 0, aAppPath);
  if (rc)
    ERRMSG(rc, "RWSCMD_OPENUSING")
  else
    rv = NS_OK;

  sRwsFreeMem(pHdr);
  return rv;
}

//------------------------------------------------------------------------
//  nsRwsService additional methods
//------------------------------------------------------------------------

// uses RwsConvert to retrieve a result that can be handled as a ULONG;
// type identifies the conversion, value can be anything (ULONG, char*, etc)

//static
nsresult
nsRwsService::RwsConvert(PRUint32 type, PRUint32 value, PRUint32 *result)
{
  nsresult  rv = NS_ERROR_FAILURE;
  PRWSHDR   pHdr = 0;

  *result = 0;
  PRUint32 rc = sRwsCall(&pHdr,
                         RWSP_CONV, 0,
                         RWSR_ASIS, 0, 1,
                         type,      0, value);

  if (rc)
    ERRMSG(rc, "RwsConvert to ULONG")
  else {
    *result = sRwsGetResult(pHdr, 1, 0);
    if (*result == (PRUint32)-1)
      *result = 0;
    else
      rv = NS_OK;
  }

  sRwsFreeMem(pHdr);
  return rv;
}

//------------------------------------------------------------------------

// uses RwsConvert to retrieve a result that can be handled as a
// Unicode string;  type identifies the conversion, value can be
// anything (ULONG, char*, etc)

//static
nsresult
nsRwsService::RwsConvert(PRUint32 type, PRUint32 value, nsAString& result)
{
  nsresult  rv = NS_ERROR_FAILURE;
  PRWSHDR   pHdr = 0;

  result.Truncate();
  PRUint32 rc = sRwsCall(&pHdr,
                         RWSP_CONV, 0,
                         RWSR_ASIS, 0, 1,
                         type,      0, value);

  if (rc)
    ERRMSG(rc, "RwsConvert to string")
  else {
    char *string = (char*)sRwsGetResult(pHdr, 1, 0);
    if (string != (char*)-1)
      rv = AssignTitleString(string, result);
  }

  sRwsFreeMem(pHdr);
  return rv;
}

//------------------------------------------------------------------------
//  nsIObserver implementation
//------------------------------------------------------------------------

// when the app shuts down, advise RwsClient;  if this is the
// last app using RwsServer, the WPS will unload the class's dll;
// also, empty the extension cache to unlock & delete its icons

NS_IMETHODIMP
nsRwsService::Observe(nsISupports *aSubject, const char *aTopic,
                      const PRUnichar *aSomeData)
{
  if (strcmp(aTopic, "quit-application") == 0) {
    PRUint32 rc = sRwsClientTerminate();
    if (rc)
        ERRMSG(rc, "RwsClientTerminate");

    if (mExtCache)
      mExtCache->EmptyCache();
  }

  return NS_OK;
}

//------------------------------------------------------------------------
// nsRwsService static helper functions
//------------------------------------------------------------------------

// this wrapper for somIsA() makes HandlerFromPath() easier to read

static nsresult IsDescendedFrom(PRUint32 wpsFilePtr, const char *pszClassname)
{
  PRWSHDR   pHdr = 0;
  nsresult  rv = NS_ERROR_FAILURE;

  PRUint32 rc = sRwsCall(&pHdr,
                         RWSP_MNAMI, (ULONG)"somIsA",
                         RWSR_ASIS,  0, 2,
                         RWSI_ASIS,  0, wpsFilePtr,
                         RWSI_CNAME, 0, pszClassname);

  if (rc)
    ERRMSG(rc, "somIsA")
  else
    if (sRwsGetResult(pHdr, 0, 0) == TRUE)
      rv = NS_OK;

  sRwsFreeMem(pHdr);
  return rv;
}

//------------------------------------------------------------------------

// create a temp file with the specified extension, then return its path

static nsresult CreateFileForExtension(const char *aFileExt,
                                       nsACString& aPath)
{
  nsresult rv = NS_ERROR_FAILURE;
  aPath.Truncate();

  nsCOMPtr<nsIFile> tempFile;
  rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(tempFile));
  if (NS_FAILED(rv))
    return rv;

  nsCAutoString pathStr(NS_LITERAL_CSTRING("nsrws."));
  if (*aFileExt == '.')
    aFileExt++;
  pathStr.Append(aFileExt);

  rv = tempFile->AppendNative(pathStr);
  if (NS_FAILED(rv))
    return rv;

  tempFile->GetNativePath(pathStr);

  FILE *fp = fopen(pathStr.get(), "wb+");
  if (fp) {
    fclose(fp);
    aPath.Assign(pathStr);
    rv = NS_OK;
  }

  return rv;
}

//------------------------------------------------------------------------

// delete a temp file created earlier

static nsresult DeleteFileForExtension(const char *aPath)
{
  if (!aPath || !*aPath)
    return NS_ERROR_INVALID_ARG;

  remove(aPath);
  return NS_OK;
}

//------------------------------------------------------------------------

// returns a localized string from unknownContentType.properties;
// if there's a failure, returns "WPS Default"

static void AssignNLSString(const PRUnichar *aKey, nsAString& result)
{
  nsresult      rv;
  nsXPIDLString title;

  do {
    nsCOMPtr<nsIStringBundleService> bundleSvc =
      mozilla::services::GetStringBundleService();
    if (!bundleSvc)
      break;

    nsCOMPtr<nsIStringBundle> bundle;
    rv = bundleSvc->CreateBundle(
      "chrome://mozapps/locale/downloads/unknownContentType.properties",
      getter_AddRefs(bundle));
    if (NS_FAILED(rv))
      break;

    // if we can't fetch the requested string, try to get "WPS Default"
    rv = bundle->GetStringFromName(aKey, getter_Copies(title));
    if (NS_FAILED(rv))
      rv = bundle->GetStringFromName(NS_LITERAL_STRING("wpsDefaultOS2").get(),
                                     getter_Copies(title));
  } while (0);

  if (NS_SUCCEEDED(rv))
    result.Assign(title);
  else
    result.Assign(NS_LITERAL_STRING("WPS Default"));
}

//------------------------------------------------------------------------

// converts a native string (presumably a WPS object's title) to
// Unicode, removes line breaks, and compresses whitespace

static nsresult AssignTitleString(const char *aTitle, nsAString& result)
{
  nsAutoChar16Buffer buffer;
  PRInt32 bufLength;

  // convert the title to Unicode
  if (NS_FAILED(MultiByteToWideChar(0, aTitle, strlen(aTitle),
                                      buffer, bufLength)))
    return NS_ERROR_FAILURE;

  PRUnichar *pSrc;
  PRUnichar *pDst;
  bool       fSkip;

  // remove line breaks, leading whitespace, & extra embedded whitespace
  // (primitive, but gcc 3.2.2 doesn't support wchar)
  for (fSkip=true, pSrc=pDst=buffer.Elements(); *pSrc; pSrc++) {
    if (*pSrc == ' ' || *pSrc == '\r' || *pSrc == '\n' || *pSrc == '\t') {
      if (!fSkip)
        *pDst++ = ' ';
      fSkip = true;
    }
    else {
      if (pDst != pSrc)
        *pDst = *pSrc;
      pDst++;
      fSkip = false;
    }
  }

  // remove the single trailing space, if needed
  if (fSkip && pDst > buffer.Elements())
    pDst--;

  *pDst = 0;
  result.Assign(buffer.Elements());
  return NS_OK;
}

//------------------------------------------------------------------------
//  ExtCache implementation
//------------------------------------------------------------------------

ExtCache::ExtCache() : mCount(0), mSize(0), mExtInfo(0)
{
  PTIB  ptib;
  PPIB  ppib;

  // the PID is needed to lock/unlock icons
  DosGetInfoBlocks(&ptib, &ppib);
  mPid = ppib->pib_ulpid;

  PRUint32 rc = DosCreateMutexSem(0, (PHMTX)&mMutex, 0, 0);
  if (rc)
    ERRMSG(rc, "DosCreateMutexSem")
}

ExtCache::~ExtCache() {}

//------------------------------------------------------------------------

// retrieve the WPS's default icon for files with this extension

nsresult ExtCache::GetIcon(const char *aExt, bool aNeedMini,
                           PRUint32 *oIcon)
{
  PRUint32 rc = DosRequestMutexSem(mMutex, kMutexTimeout);
  if (rc) {
    ERRMSG(rc, "DosRequestMutexSem")
    return NS_ERROR_FAILURE;
  }

  ExtInfo *info = FindExtension(aExt);

  if (info) {
    if (aNeedMini)
      *oIcon = info->mini;
    else
      *oIcon = info->icon;
  }
  else
    *oIcon = 0;

  rc = DosReleaseMutexSem(mMutex);
  if (rc)
    ERRMSG(rc, "DosReleaseMutexSem")

  return (*oIcon ? NS_OK : NS_ERROR_FAILURE);
}

//------------------------------------------------------------------------

// save the WPS's default icon for files with this extension

nsresult ExtCache::SetIcon(const char *aExt, bool aIsMini,
                           PRUint32 aIcon)
{
  PRUint32 rc = DosRequestMutexSem(mMutex, kMutexTimeout);
  if (rc) {
    ERRMSG(rc, "DosRequestMutexSem")
    return NS_ERROR_FAILURE;
  }

  ExtInfo *info = FindExtension(aExt, true);
  if (!info)
    return NS_ERROR_FAILURE;

  // the icon has to be made non-deletable or else
  // it will be destroyed if the WPS terminates
  if (!WinSetPointerOwner(aIcon, mPid, FALSE)) {
    ERRPRINTF(info->ext, "WinSetPointerOwner failed for %s icon")
    return NS_ERROR_FAILURE;
  }

  if (aIsMini)
    info->mini = aIcon;
  else
    info->icon = aIcon;

  ERRPRINTF(info->ext, "ExtCache - added icon for %s")

  rc = DosReleaseMutexSem(mMutex);
  if (rc)
    ERRMSG(rc, "DosReleaseMutexSem")

  return NS_OK;
}

//------------------------------------------------------------------------

// retrieve the WPS default handler's title & object handle (if any)

nsresult ExtCache::GetHandler(const char *aExt, PRUint32 *oHandle,
                              nsAString& oTitle)
{
  PRUint32 rc = DosRequestMutexSem(mMutex, kMutexTimeout);
  if (rc) {
    ERRMSG(rc, "DosRequestMutexSem")
    return NS_ERROR_FAILURE;
  }

  nsresult rv = NS_ERROR_FAILURE;
  ExtInfo *info = FindExtension(aExt);

  // if there's no title, the handle isn't useful
  if (info && info->title) {
    oTitle.Assign(info->title);
    *oHandle = info->handler;
    rv = NS_OK;
  }

  rc = DosReleaseMutexSem(mMutex);
  if (rc)
    ERRMSG(rc, "DosReleaseMutexSem")

  return rv;
}

//------------------------------------------------------------------------

// save the WPS default handler's title & object handle (if any)

nsresult ExtCache::SetHandler(const char *aExt, PRUint32 aHandle,
                              nsAString& aTitle)
{
  PRUint32 rc = DosRequestMutexSem(mMutex, kMutexTimeout);
  if (rc) {
    ERRMSG(rc, "DosRequestMutexSem")
    return NS_ERROR_FAILURE;
  }

  nsresult rv = NS_ERROR_FAILURE;
  ExtInfo *info = FindExtension(aExt, true);

  // if the title can't be saved, don't save the handle
  if (info) {
    info->title = ToNewUnicode(aTitle);
    if (info->title) {
      info->handler = aHandle;
      rv = NS_OK;
      ERRPRINTF(info->ext, "ExtCache - added handler for %s")
    }
  }

  rc = DosReleaseMutexSem(mMutex);
  if (rc)
    ERRMSG(rc, "DosReleaseMutexSem")

  return rv;
}

//------------------------------------------------------------------------

// find the entry for the requested extension;  if not found,
// create a new entry, expanding the array as needed

ExtInfo *ExtCache::FindExtension(const char *aExt, bool aSet)
{
  // eliminate any leading dot & null extensions
  if (*aExt == '.')
    aExt++;
  if (*aExt == 0)
    return 0;

  // too long to cache
  if (strlen(aExt) >= 8)
    return 0;

  // uppercase extension, then confirm it still fits
  char extUpper[16];
  strcpy(extUpper, aExt);
  // XXX WinUpper() can crash with high-memory
  //     could we just store as-is and instead use stricmp() for
  //     compare operations?
  if (WinUpper(0, 0, 0, extUpper) >= 8)
    return 0;

  // a minor optimization:  if we're setting a value, it probably
  // belongs to the entry added most recently (i.e. the last one)
  if (aSet && mCount && !strcmp(extUpper, (&mExtInfo[mCount-1])->ext))
    return &mExtInfo[mCount-1];

  ExtInfo *info;
  PRUint32  ctr;

  // look for the extension in the array, return if found
  for (ctr = 0, info = mExtInfo; ctr < mCount; ctr++, info++)
    if (!strcmp(extUpper, info->ext))
      return info;

  // if a new entry won't fit into the current array, realloc
  if (mCount >= mSize) {
    PRUint32 newSize = mSize + kGrowBy;
    info = (ExtInfo*) NS_Realloc(mExtInfo, newSize * sizeof(ExtInfo));
    if (!info)
      return 0;

    memset(&info[mSize], 0, kGrowBy * sizeof(ExtInfo));
    mExtInfo = info;
    mSize = newSize;
  }

  // point at the next entry & store the extension
  info = &mExtInfo[mCount++];
  strcpy(info->ext, extUpper);

  return info;
}

//------------------------------------------------------------------------

// clear out the cache - since this is only called at shutdown,
// the primary concern is that the icons get unlocked & deleted

void ExtCache::EmptyCache()
{
  if (!mExtInfo)
    return;

  PRUint32 rc = DosRequestMutexSem(mMutex, kMutexTimeout);
  if (rc) {
    ERRMSG(rc, "DosRequestMutexSem")
    return;
  }

  PRUint32  saveMutex = mMutex;
  mMutex = 0;

  PRUint32 ctr;
  ExtInfo *info;

  for (ctr = 0, info = mExtInfo; ctr < mCount; ctr++, info++) {

    ERRPRINTF(info->ext, "ExtCache - deleting entry for %s")

    if (info->icon) {
      if (WinSetPointerOwner(info->icon, mPid, TRUE) == FALSE ||
          WinDestroyPointer(info->icon) == FALSE)
        ERRPRINTF(info->ext, "unable to destroy icon for %s")
    }

    if (info->mini) {
      if (WinSetPointerOwner(info->mini, mPid, TRUE) == FALSE ||
          WinDestroyPointer(info->mini) == FALSE)
        ERRPRINTF(info->ext, "unable to destroy mini for %s")
    }

    if (info->title)
      NS_Free(info->title);
  }

  mCount = 0;
  mSize = 0;
  NS_Free(mExtInfo);
  mExtInfo = 0;

  rc = DosReleaseMutexSem(saveMutex);
  if (rc)
    ERRMSG(rc, "DosReleaseMutexSem")
  rc = DosCloseMutexSem(saveMutex);
  if (rc)
    ERRMSG(rc, "DosCloseMutexSem")
}

//------------------------------------------------------------------------
//  Module & Factory stuff
//------------------------------------------------------------------------

// this is the "getter proc" for nsRwsServiceConstructor();  it makes a
// single attempt to load the RWS libraries and, if successful, creates
// our singleton object;  thereafter, it returns the existing object or
// NS_ERROR_NOT_AVAILABLE

static nsresult nsRwsServiceInit(nsRwsService **aClass)
{
  // init already done - return what we've got or an error
  if (sInit) {
    *aClass = sRwsInstance;
    if (*aClass == 0)
      return NS_ERROR_NOT_AVAILABLE;

    NS_ADDREF(*aClass);
    return NS_OK;
  }

  sInit = TRUE;
  *aClass = 0;

  // don't load RWS if "MOZ_NO_RWS" is found in the environment
  if (PR_GetEnv("MOZ_NO_RWS"))
    return NS_ERROR_NOT_AVAILABLE;

  char      errBuf[16];
  HMODULE   hmod;

  // try to load RwsCliXX.dll;  first, see if the RWS WPS class is
  // registered by f/q name - if so, look for RwsCli in the same
  // directory;  the goal is to consistently use the same pair of
  // dlls if the user has multiple copies

  PRUint32  rc = 1;

  // get the list of registered WPS classes
  ULONG  cbClass;
  if (!WinEnumObjectClasses(NULL, &cbClass))
    return NS_ERROR_NOT_AVAILABLE;

  char *pBuf = (char*)NS_Alloc(cbClass + CCHMAXPATH);
  if (!pBuf)
    return NS_ERROR_OUT_OF_MEMORY;

  POBJCLASS pClass = (POBJCLASS)&pBuf[CCHMAXPATH];
  if (!WinEnumObjectClasses(pClass, &cbClass)) {
    NS_Free(pBuf);
    return NS_ERROR_NOT_AVAILABLE;
  }

  // look for RWSxx
  while (pClass) {
    if (!strcmp(pClass->pszClassName, RWSCLASSNAME))
      break;
    pClass = pClass->pNext;
  }

  // if the class was found & it was registered with a f/q name,
  // try to load RwsCliXX from the same directory
  if (pClass && pClass->pszModName[1] == ':') {
    strcpy(pBuf, pClass->pszModName);
    char *ptr = strrchr(pBuf, '\\');
    if (ptr) {
      strcpy(ptr+1, RWSCLIDLL);
      rc = DosLoadModule(errBuf, sizeof(errBuf), pBuf, &hmod);
    }
  }
  NS_Free(pBuf);

  // if RwsCli couldn't be found, look for it on the LIBPATH
  if (rc)
    rc = DosLoadModule(errBuf, sizeof(errBuf), RWSCLIMOD, &hmod);

  // the dll couldn't be found, so exit
  if (rc) {
    ERRPRINTF(RWSCLIDLL, "nsRwsServiceInit - unable to locate %s");
    return NS_ERROR_NOT_AVAILABLE;
  }

  // get the addresses of the procs we'll be using;
  if (DosQueryProcAddr(hmod, ORD_RWSCALL,        0, (PFN*)&sRwsCall) ||
      DosQueryProcAddr(hmod, ORD_RWSCALLINDIRECT,0, (PFN*)&sRwsCallIndirect) ||
      DosQueryProcAddr(hmod, ORD_RWSFREEMEM,     0, (PFN*)&sRwsFreeMem) ||
      DosQueryProcAddr(hmod, ORD_RWSGETRESULT,   0, (PFN*)&sRwsGetResult) ||
      DosQueryProcAddr(hmod, ORD_RWSGETARGPTR,   0, (PFN*)&sRwsGetArgPtr) ||
      DosQueryProcAddr(hmod, ORD_RWSCLIENTINIT,  0, (PFN*)&sRwsClientInit) ||
      DosQueryProcAddr(hmod, ORD_RWSGETTIMEOUT,  0, (PFN*)&sRwsGetTimeout) ||
      DosQueryProcAddr(hmod, ORD_RWSSETTIMEOUT,  0, (PFN*)&sRwsSetTimeout) ||
      DosQueryProcAddr(hmod, ORD_RWSCLIENTTERMINATE, 0, (PFN*)&sRwsClientTerminate)) {
    DosFreeModule(hmod);
    ERRPRINTF("", "nsRwsServiceInit - DosQueryProcAddr failed%s")
    return NS_ERROR_NOT_AVAILABLE;
  }

  // init RWS and have it register the WPS class if needed
  rc = sRwsClientInit(TRUE);
  if (rc) {
    ERRMSG(rc, "RwsClientInit")
    return NS_ERROR_NOT_AVAILABLE;
  }

  // if the user hasn't set a timeout, reset it to 2 seconds
  // (the default is 20 seconds)
  PRUint32  currentTO;
  PRUint32  userTO;
  rc = sRwsGetTimeout((PULONG)&currentTO, (PULONG)&userTO);
  if (rc)
    ERRMSG(rc, "RwsGetTimeout")
  else
    if (userTO == 0) {
      rc = sRwsSetTimeout(2);
      if (rc)
        ERRMSG(rc, "RwsSetTimeout")
    }

  // create an instance of nsRwsService
  sRwsInstance = new nsRwsService();
  if (sRwsInstance == 0)
    return NS_ERROR_OUT_OF_MEMORY;

  *aClass = sRwsInstance;
  NS_ADDREF(*aClass);

  // set the class up as a shutdown observer
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os)
    os->AddObserver(*aClass, "quit-application", false);

  return NS_OK;
}

//------------------------------------------------------------------------

// this is a variation on NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR();
// the only difference is that the _GetterProc returns an nsresult to
// provide a more appropriate error code (typically NS_ERROR_NOT_AVAILABLE)

NS_IMETHODIMP nsRwsServiceConstructor(nsISupports *aOuter, REFNSIID aIID,
                                       void **aResult)
{
  nsresult rv;
  nsRwsService *inst;
  *aResult = NULL;

  if (aOuter) {
    rv = NS_ERROR_NO_AGGREGATION;
    return rv;
  }

  rv = nsRwsServiceInit(&inst);
  if (NS_FAILED(rv)) {
    ERRPRINTF(rv, "==>> nsRwsServiceInit failed - rv= %x");
    return rv;
  }

  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

  return rv;
}

//------------------------------------------------------------------------
