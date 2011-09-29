/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 et cin: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <mscott@netscape.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>  (Added mailcap and mime.types support)
 *   Christian Biesinger <cbiesinger@web.de>
 *   Rich Walsh <dragtext@e-vertise.com>
 *   Peter Weilbacher <mozilla@Weilbacher.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef MOZ_OS2_HIGH_MEMORY
// os2safe.h has to be included before os2.h, needed for high mem
#include <os2safe.h>
#endif

#include "nsMIMEInfoOS2.h"
#include "nsOSHelperAppService.h"
#include "nsExternalHelperAppService.h"
#include "nsCExternalHandlerService.h"
#include "nsReadableUtils.h"
#include "nsIProcess.h"
#include "nsNetUtil.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIVariant.h"
#include "nsArrayEnumerator.h"
#include "nsIRwsService.h"
#include <stdlib.h>
#include "mozilla/Preferences.h"

using namespace mozilla;

//------------------------------------------------------------------------

#define SALT_SIZE 8
#define TABLE_SIZE 36
static const PRUnichar table[] = 
  { 'a','b','c','d','e','f','g','h','i','j',
    'k','l','m','n','o','p','q','r','s','t',
    'u','v','w','x','y','z','0','1','2','3',
    '4','5','6','7','8','9'};

// reduces overhead by preventing calls to nsRwsService when it isn't present
static bool sUseRws = true;

//------------------------------------------------------------------------

NS_IMPL_ISUPPORTS_INHERITED1(nsMIMEInfoOS2, nsMIMEInfoBase, nsIPropertyBag)

nsMIMEInfoOS2::~nsMIMEInfoOS2()
{
}

//------------------------------------------------------------------------
// if the helper application is a DOS app, create an 8.3 filename
static nsresult Make8Dot3Name(nsIFile *aFile, nsACString& aPath)
{
  nsCAutoString leafName;
  aFile->GetNativeLeafName(leafName);
  const char *lastDot = strrchr(leafName.get(), '.');

  char suffix[8] = "";
  if (lastDot) {
    strncpy(suffix, lastDot, 4);
    suffix[4] = '\0';
  }

  nsCOMPtr<nsIFile> tempPath;
  nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(tempPath));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString saltedTempLeafName;
  do {
    saltedTempLeafName.Truncate();

    // this salting code was ripped directly from the profile manager.
    // turn PR_Now() into milliseconds since epoch 1058 & salt rand with that
    double fpTime;
    LL_L2D(fpTime, PR_Now());
    srand((uint)(fpTime * 1e-6 + 0.5));

    for (PRInt32 i=0; i < SALT_SIZE; i++)
      saltedTempLeafName.Append(table[(rand()%TABLE_SIZE)]);

    AppendASCIItoUTF16(suffix, saltedTempLeafName);
    rv = aFile->CopyTo(tempPath, saltedTempLeafName);
  } while (NS_FAILED(rv));

  nsCOMPtr<nsPIExternalAppLauncher>
    helperAppService(do_GetService(NS_EXTERNALHELPERAPPSERVICE_CONTRACTID));
  if (!helperAppService)
    return NS_ERROR_FAILURE;

  tempPath->Append(saltedTempLeafName);
  helperAppService->DeleteTemporaryFileOnExit(tempPath);
  tempPath->GetNativePath(aPath);

  return rv;
}

//------------------------------------------------------------------------

// opens a file using the selected program or WPS object

NS_IMETHODIMP nsMIMEInfoOS2::LaunchWithFile(nsIFile *aFile)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIFile> application;
  if (mPreferredAction == useHelperApp) {
    nsCOMPtr<nsILocalHandlerApp> localHandlerApp =
      do_QueryInterface(mPreferredApplication, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = localHandlerApp->GetExecutable(getter_AddRefs(application));
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (mPreferredAction == useSystemDefault) {
    application = mDefaultApplication;
  } else {
    return NS_ERROR_INVALID_ARG;
  }

  nsCAutoString filePath;
  aFile->GetNativePath(filePath);

  // if there's no program, use the WPS to open the file
  if (!application) {
    rv = NS_ERROR_FAILURE;

    // if RWS is enabled, see if nsOSHelperAppService provided a handle for
    // the app associated with this file;  if so, use it to open the file;
    if (sUseRws) {
      PRUint32 appHandle;
      GetDefaultAppHandle(&appHandle);
      if (appHandle) {
        nsCOMPtr<nsIRwsService> rwsSvc(do_GetService("@mozilla.org/rwsos2;1"));
        if (!rwsSvc) {
          sUseRws = PR_FALSE;
        } else {
          // this call is identical to dropping the file on a program's icon;
          // it ensures filenames with multiple dots are handled correctly
          rv = rwsSvc->OpenWithAppHandle(filePath.get(), appHandle);
        }
      }
    }

    // if RWS isn't present or fails, open it using a PM call
    if (NS_FAILED(rv)) {
      if (WinSetObjectData(WinQueryObject(filePath.get()), "OPEN=DEFAULT"))
        rv = NS_OK;
    }

    return rv;
  }

  // open the data file using the specified program file
  nsCAutoString appPath;
  if (application) {
    application->GetNativePath(appPath);
  }

  ULONG ulAppType;
  DosQueryAppType(appPath.get(), &ulAppType);
  if (ulAppType & (FAPPTYP_DOS |
                   FAPPTYP_WINDOWSPROT31 |
                   FAPPTYP_WINDOWSPROT |
                   FAPPTYP_WINDOWSREAL)) {
    rv = Make8Dot3Name(aFile, filePath);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  filePath.Insert('\"', 0);
  filePath.Append('\"');

  // if RWS is enabled, have the WPS open the file using the selected app;
  // this lets the user specify commandline args in the exe's WPS notebook
  rv = NS_ERROR_FAILURE;
  if (sUseRws) {
    nsCOMPtr<nsIRwsService> rwsSvc(do_GetService("@mozilla.org/rwsos2;1"));
    if (!rwsSvc) {
      sUseRws = PR_FALSE;
    } else {
      rv = rwsSvc->OpenWithAppPath(filePath.get(), appPath.get());
    }
  }

  // if RWS isn't present or fails, use Moz facilities to run the program
  if (NS_FAILED(rv)) {
    nsCOMPtr<nsIProcess> process = do_CreateInstance(NS_PROCESS_CONTRACTID);
    if (NS_FAILED(rv = process->Init(application)))
      return rv;
    const char *strPath = filePath.get();
    return process->Run(PR_FALSE, &strPath, 1);
  }

  return rv;
}

//------------------------------------------------------------------------

// if there's a description, there's a handler (which may be the WPS)

NS_IMETHODIMP nsMIMEInfoOS2::GetHasDefaultHandler(bool *_retval)
{
  *_retval = !mDefaultAppDescription.IsEmpty();
  return NS_OK;
}

//------------------------------------------------------------------------

// copied directly from nsMIMEInfoImpl

NS_IMETHODIMP
nsMIMEInfoOS2::GetDefaultDescription(nsAString& aDefaultDescription)
{
  if (mDefaultAppDescription.IsEmpty() && mDefaultApplication)
    mDefaultApplication->GetLeafName(aDefaultDescription);
  else
    aDefaultDescription = mDefaultAppDescription;

  return NS_OK;
}

//------------------------------------------------------------------------

// Get() is new, Set() is an override;  they permit nsOSHelperAppService
// to reorder the default & preferred app handlers

void nsMIMEInfoOS2::GetDefaultApplication(nsIFile **aDefaultAppHandler)
{
  *aDefaultAppHandler = mDefaultApplication;
  NS_IF_ADDREF(*aDefaultAppHandler);
  return;
}

void nsMIMEInfoOS2::SetDefaultApplication(nsIFile *aDefaultApplication)
{
  mDefaultApplication = aDefaultApplication;
  return;
}

//------------------------------------------------------------------------

// gets/sets the handle of the WPS object associated with this mimetype

void nsMIMEInfoOS2::GetDefaultAppHandle(PRUint32 *aHandle)
{
  if (aHandle) {
    if (mDefaultAppHandle <= 0x10000 || mDefaultAppHandle >= 0x40000)
      mDefaultAppHandle = 0;
    *aHandle = mDefaultAppHandle;
  }
  return;
}

void nsMIMEInfoOS2::SetDefaultAppHandle(PRUint32 aHandle)
{
  if (aHandle <= 0x10000 || aHandle >= 0x40000)
    mDefaultAppHandle = 0;
  else
    mDefaultAppHandle = aHandle;
  return;
}

//------------------------------------------------------------------------

nsresult nsMIMEInfoOS2::LoadUriInternal(nsIURI *aURL)
{
  nsresult rv;
  NS_ENSURE_TRUE(Preferences::GetRootBranch(), NS_ERROR_FAILURE);

  /* Convert SimpleURI to StandardURL */
  nsCOMPtr<nsIURI> uri = do_CreateInstance(NS_STANDARDURL_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }
  nsCAutoString urlSpec;
  aURL->GetSpec(urlSpec);
  uri->SetSpec(urlSpec);

  /* Get the protocol so we can look up the preferences */
  nsCAutoString uProtocol;
  uri->GetScheme(uProtocol);

  nsCAutoString branchName = NS_LITERAL_CSTRING("applications.") + uProtocol;
  nsCAutoString prefName = branchName + branchName;
  nsAdoptingCString prefString = Preferences::GetCString(prefName.get());

  nsCAutoString applicationName;
  nsCAutoString parameters;

  if (prefString.IsEmpty()) {
    char szAppFromINI[CCHMAXPATH];
    char szParamsFromINI[MAXINIPARAMLENGTH];
    /* did OS2.INI contain application? */
    rv = GetApplicationAndParametersFromINI(uProtocol,
                                            szAppFromINI, sizeof(szAppFromINI),
                                            szParamsFromINI, sizeof(szParamsFromINI));
    if (NS_SUCCEEDED(rv)) {
      applicationName = szAppFromINI;
      parameters = szParamsFromINI;
    } else {
      return NS_ERROR_FAILURE;
    }
  }

  // Dissect the URI
  nsCAutoString uURL, uUsername, uPassword, uHost, uPort, uPath;
  nsCAutoString uEmail, uGroup;
  PRInt32 iPort;

  // when passing to OS/2 apps later, we need ASCII URLs,
  // UTF-8 would probably not get handled correctly
  aURL->GetAsciiSpec(uURL);
  uri->GetAsciiHost(uHost);
  uri->GetUsername(uUsername);
  NS_UnescapeURL(uUsername);
  uri->GetPassword(uPassword);
  NS_UnescapeURL(uPassword);
  uri->GetPort(&iPort);
  /* GetPort returns -1 if there is no port in the URI */
  if (iPort != -1)
    uPort.AppendInt(iPort);
  uri->GetPath(uPath);
  NS_UnescapeURL(uPath);

  // One could use nsIMailtoUrl to get email and newsgroup,
  // but it is probably easier to do that quickly by hand here
  // uEmail is both email address and message id  for news
  uEmail = uUsername + NS_LITERAL_CSTRING("@") + uHost;
  // uPath can almost be used as newsgroup and as channel for IRC
  // but strip leading "/"
  uGroup = Substring(uPath, 1, uPath.Length());

  NS_NAMED_LITERAL_CSTRING(url, "%url%");
  NS_NAMED_LITERAL_CSTRING(username, "%username%");
  NS_NAMED_LITERAL_CSTRING(password, "%password%");
  NS_NAMED_LITERAL_CSTRING(host, "%host%");
  NS_NAMED_LITERAL_CSTRING(port, "%port%");
  NS_NAMED_LITERAL_CSTRING(email, "%email%");
  NS_NAMED_LITERAL_CSTRING(group, "%group%");
  NS_NAMED_LITERAL_CSTRING(msgid, "%msgid%");
  NS_NAMED_LITERAL_CSTRING(channel, "%channel%");

  bool replaced = false;
  if (applicationName.IsEmpty() && parameters.IsEmpty()) {
    /* Put application name in parameters */
    applicationName.Append(prefString);

    branchName.Append(".");
    prefName = branchName + NS_LITERAL_CSTRING("parameters");
    prefString = Preferences::GetCString(prefName.get());
    /* If parameters have been specified, use them instead of the separate entities */
    if (!prefString.IsEmpty()) {
      parameters.Append(" ");
      parameters.Append(prefString);

      PRInt32 pos = parameters.Find(url.get());
      if (pos != kNotFound) {
        nsCAutoString uURL;
        aURL->GetSpec(uURL);
        NS_UnescapeURL(uURL);
        uURL.Cut(0, uProtocol.Length()+1);
        parameters.Replace(pos, url.Length(), uURL);
        replaced = PR_TRUE;
      }
    } else {
      /* port */
      if (!uPort.IsEmpty()) {
        prefName = branchName + NS_LITERAL_CSTRING("port");
        prefString = Preferences::GetCString(prefName.get());
        if (!prefString.IsEmpty()) {
          parameters.Append(" ");
          parameters.Append(prefString);
        }
      }
      /* username */
      if (!uUsername.IsEmpty()) {
        prefName = branchName + NS_LITERAL_CSTRING("username");
        prefString = Preferences::GetCString(prefName.get());
        if (!prefString.IsEmpty()) {
          parameters.Append(" ");
          parameters.Append(prefString);
        }
      }
      /* password */
      if (!uPassword.IsEmpty()) {
        prefName = branchName + NS_LITERAL_CSTRING("password");
        prefString = Preferences::GetCString(prefName.get());
        if (!prefString.IsEmpty()) {
          parameters.Append(" ");
          parameters.Append(prefString);
        }
      }
      /* host */
      if (!uHost.IsEmpty()) {
        prefName = branchName + NS_LITERAL_CSTRING("host");
        prefString = Preferences::GetCString(prefName.get());
        if (!prefString.IsEmpty()) {
          parameters.Append(" ");
          parameters.Append(prefString);
        }
      }
    }
  }

#ifdef DEBUG_peter
  printf("uURL=%s\n", uURL.get());
  printf("uUsername=%s\n", uUsername.get());
  printf("uPassword=%s\n", uPassword.get());
  printf("uHost=%s\n", uHost.get());
  printf("uPort=%s\n", uPort.get());
  printf("uPath=%s\n", uPath.get());
  printf("uEmail=%s\n", uEmail.get());
  printf("uGroup=%s\n", uGroup.get());
#endif

  PRInt32 pos;
  pos = parameters.Find(url.get());
  if (pos != kNotFound) {
    replaced = PR_TRUE;
    parameters.Replace(pos, url.Length(), uURL);
  }
  pos = parameters.Find(username.get());
  if (pos != kNotFound) {
    replaced = PR_TRUE;
    parameters.Replace(pos, username.Length(), uUsername);
  }
  pos = parameters.Find(password.get());
  if (pos != kNotFound) {
    replaced = PR_TRUE;
    parameters.Replace(pos, password.Length(), uPassword);
  }
  pos = parameters.Find(host.get());
  if (pos != kNotFound) {
    replaced = PR_TRUE;
    parameters.Replace(pos, host.Length(), uHost);
  }
  pos = parameters.Find(port.get());
  if (pos != kNotFound) {
    replaced = PR_TRUE;
    parameters.Replace(pos, port.Length(), uPort);
  }
  pos = parameters.Find(email.get());
  if (pos != kNotFound) {
    replaced = PR_TRUE;
    parameters.Replace(pos, email.Length(), uEmail);
  }
  pos = parameters.Find(group.get());
  if (pos != kNotFound) {
    replaced = PR_TRUE;
    parameters.Replace(pos, group.Length(), uGroup);
  }
  pos = parameters.Find(msgid.get());
  if (pos != kNotFound) {
    replaced = PR_TRUE;
    parameters.Replace(pos, msgid.Length(), uEmail);
  }
  pos = parameters.Find(channel.get());
  if (pos != kNotFound) {
    replaced = PR_TRUE;
    parameters.Replace(pos, channel.Length(), uGroup);
  }
  // If no replacement variable was used, the user most likely uses the WPS URL
  // object and does not know about the replacement variables.
  // Just append the full URL.
  if (!replaced) {
    parameters.Append(" ");
    parameters.Append(uURL);
  }

  const char *params[3];
  params[0] = parameters.get();
#ifdef DEBUG_peter
  printf("params[0]=%s\n", params[0]);
#endif
  PRInt32 numParams = 1;

  nsCOMPtr<nsILocalFile> application;
  rv = NS_NewNativeLocalFile(nsDependentCString(applicationName.get()), PR_FALSE, getter_AddRefs(application));
  if (NS_FAILED(rv)) {
     /* Maybe they didn't qualify the name - search path */
     char szAppPath[CCHMAXPATH];
     APIRET rc = DosSearchPath(SEARCH_IGNORENETERRS | SEARCH_ENVIRONMENT,
                               "PATH", applicationName.get(),
                               szAppPath, sizeof(szAppPath));
     if (rc == NO_ERROR) {
       rv = NS_NewNativeLocalFile(nsDependentCString(szAppPath), PR_FALSE, getter_AddRefs(application));
     }
     if (NS_FAILED(rv) || (rc != NO_ERROR)) {
       /* Try just launching it with COMSPEC */
       rv = NS_NewNativeLocalFile(nsDependentCString(getenv("COMSPEC")), PR_FALSE, getter_AddRefs(application));
       if (NS_FAILED(rv)) {
         return rv;
       }

       params[0] = "/c";
       params[1] = applicationName.get();
       params[2] = parameters.get();
       numParams = 3;
     }
  }

  nsCOMPtr<nsIProcess> process = do_CreateInstance(NS_PROCESS_CONTRACTID);

  if (NS_FAILED(rv = process->Init(application)))
     return rv;

  if (NS_FAILED(rv = process->Run(PR_FALSE, params, numParams)))
    return rv;

  return NS_OK;
}

//------------------------------------------------------------------------
// nsIPropertyBag
//------------------------------------------------------------------------

NS_IMETHODIMP
nsMIMEInfoOS2::GetEnumerator(nsISimpleEnumerator **_retval)
{
  nsCOMArray<nsIVariant> properties;

  nsCOMPtr<nsIVariant> variant;
  GetProperty(NS_LITERAL_STRING("defaultApplicationIconURL"), getter_AddRefs(variant));
  if (variant)
    properties.AppendObject(variant);

  GetProperty(NS_LITERAL_STRING("customApplicationIconURL"), getter_AddRefs(variant));
  if (variant)
    properties.AppendObject(variant);

  return NS_NewArrayEnumerator(_retval, properties);
}

//------------------------------------------------------------------------

NS_IMETHODIMP
nsMIMEInfoOS2::GetProperty(const nsAString& aName, nsIVariant **_retval)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (aName.EqualsLiteral(PROPERTY_DEFAULT_APP_ICON_URL)) {
    rv = GetIconURLVariant(mDefaultApplication, _retval);
  } else {
    if (aName.EqualsLiteral(PROPERTY_CUSTOM_APP_ICON_URL) &&
        mPreferredApplication) {
      // find file from handler
      nsCOMPtr<nsIFile> appFile;
      nsCOMPtr<nsILocalHandlerApp> localHandlerApp =
        do_QueryInterface(mPreferredApplication, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = localHandlerApp->GetExecutable(getter_AddRefs(appFile));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = GetIconURLVariant(appFile, _retval);
    }
  }

  return rv;
}

//------------------------------------------------------------------------

NS_IMETHODIMP
nsMIMEInfoOS2::GetIconURLVariant(nsIFile *aApplication, nsIVariant **_retval)
{
  nsresult rv = CallCreateInstance("@mozilla.org/variant;1", _retval);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString fileURLSpec;
  if (aApplication)
    NS_GetURLSpecFromFile(aApplication, fileURLSpec);
  else {
    GetPrimaryExtension(fileURLSpec);
    fileURLSpec.Insert(NS_LITERAL_CSTRING("moztmp."), 0);
  }

  nsCAutoString iconURLSpec(NS_LITERAL_CSTRING("moz-icon://"));
  iconURLSpec += fileURLSpec;
  nsCOMPtr<nsIWritableVariant> writable(do_QueryInterface(*_retval));
  writable->SetAsAUTF8String(iconURLSpec);

  return NS_OK;
}
