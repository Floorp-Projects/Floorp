/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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

#include "nsMIMEInfoOS2.h"
#include "nsExternalHelperAppService.h"
#include "nsCExternalHandlerService.h"
#include "nsReadableUtils.h"
#include "nsIProcess.h"
#include <stdlib.h>

#define SALT_SIZE 8
#define TABLE_SIZE 36
static const PRUnichar table[] = 
  { 'a','b','c','d','e','f','g','h','i','j',
    'k','l','m','n','o','p','q','r','s','t',
    'u','v','w','x','y','z','0','1','2','3',
    '4','5','6','7','8','9'};


nsMIMEInfoOS2::~nsMIMEInfoOS2()
{
}

NS_IMETHODIMP nsMIMEInfoOS2::LaunchWithURI(nsIURI* aURI,
                                           nsIInterfaceRequestor* aWindowContext)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsILocalFile> docToLoad;
  rv = GetLocalFileFromURI(aURI, getter_AddRefs(docToLoad));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString path;
  docToLoad->GetNativePath(path);
  
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

  // The nsIMIMEInfo should have either the default or preferred 
  // application handler attribute set to match the preferredAction!
  if (!application) {
    HOBJECT hobject = WinQueryObject(path.get());
    if (WinSetObjectData( hobject, "OPEN=DEFAULT" ))
      return NS_OK;
    else
      return NS_ERROR_FAILURE;
  }
  
  ULONG ulAppType;
  nsCAutoString apppath;
  application->GetNativePath(apppath);
  DosQueryAppType(apppath.get(), &ulAppType);
  if (ulAppType & (FAPPTYP_DOS |
                   FAPPTYP_WINDOWSPROT31 |
                   FAPPTYP_WINDOWSPROT |
                   FAPPTYP_WINDOWSREAL)) {
    // if the helper application is a DOS app, create an 8.3 filename
    // we do this even if the filename is valid because it's 8.3, who cares
    nsCOMPtr<nsPIExternalAppLauncher> helperAppService (do_GetService(NS_EXTERNALHELPERAPPSERVICE_CONTRACTID));
    if (helperAppService)
    {
      nsCAutoString leafName; 
      docToLoad->GetNativeLeafName(leafName);
      const char* lastDot = strrchr(leafName.get(), '.');
      char suffix[CCHMAXPATH + 1] = "";
      if (lastDot)
      {
          strcpy(suffix, lastDot);
      }
      suffix[4] = '\0';
      
      nsAutoString saltedTempLeafName;
      do {
          saltedTempLeafName.Truncate();
          // this salting code was ripped directly from the profile manager.
          // turn PR_Now() into milliseconds since epoch 1058 // and salt rand with that. 
          double fpTime;
          LL_L2D(fpTime, PR_Now());
          srand((uint)(fpTime * 1e-6 + 0.5));
          PRInt32 i;
          for (i=0;i<SALT_SIZE;i++) {
            saltedTempLeafName.Append(table[(rand()%TABLE_SIZE)]);
          }
          AppendASCIItoUTF16(suffix, saltedTempLeafName);
          rv = docToLoad->MoveTo(nsnull, saltedTempLeafName);
      } while (NS_FAILED(rv));
      helperAppService->DeleteTemporaryFileOnExit(docToLoad);
      docToLoad->GetNativePath(path);
    }
  } else {
    path.Insert('\"', 0);
    path.Append('\"');
  }
    
  const char * strPath = path.get();
  // if we were given an application to use then use it....otherwise
  // make the registry call to launch the app
  nsCOMPtr<nsIProcess> process = do_CreateInstance(NS_PROCESS_CONTRACTID);
  if (NS_FAILED(rv = process->Init(application)))
    return rv;
  PRUint32 pid;
  return process->Run(PR_FALSE, &strPath, 1, &pid);
}

nsresult nsMIMEInfoOS2::LoadUriInternal(nsIURI * aURL)
{
// XXX this is just a build break fix, functionality is broken, see bug 390075
#warning nsMIMEInfoOS2::LoadUriInternal is dysfunctional!
//  LOG(("-- nsOSHelperAppService::LoadUriInternal\n"));
  nsCOMPtr<nsIPref> thePrefsService(do_GetService(NS_PREF_CONTRACTID));
  if (!thePrefsService) {
    return NS_ERROR_FAILURE;
  }

  /* Convert SimpleURI to StandardURL */
  nsresult rv;
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

  nsCAutoString prefName;
  prefName = NS_LITERAL_CSTRING("applications.") + uProtocol;
  nsXPIDLCString prefString;

  nsCAutoString applicationName;
  nsCAutoString parameters;

  rv = thePrefsService->CopyCharPref(prefName.get(), getter_Copies(prefString));
  if (NS_FAILED(rv) || prefString.IsEmpty()) {
    char szAppFromINI[CCHMAXPATH];
    char szParamsFromINI[MAXINIPARAMLENGTH];
    /* did OS2.INI contain application? */
// XXX this is just a build break fix, functionality is broken, see bug 390075
//    rv = GetApplicationAndParametersFromINI(uProtocol,
//                                            szAppFromINI, sizeof(szAppFromINI),
//                                            szParamsFromINI, sizeof(szParamsFromINI));
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

  PRBool replaced = PR_FALSE;
  if (applicationName.IsEmpty() && parameters.IsEmpty()) {
    /* Put application name in parameters */
    applicationName.Append(prefString);

    prefName.Append(".");
    nsCOMPtr<nsIPrefBranch> prefBranch;
    rv = thePrefsService->GetBranch(prefName.get(), getter_AddRefs(prefBranch));
    if (NS_SUCCEEDED(rv) && prefBranch) {
      rv = prefBranch->GetCharPref("parameters", getter_Copies(prefString));
      /* If parameters have been specified, use them instead of the separate entities */
      if (NS_SUCCEEDED(rv) && !prefString.IsEmpty()) {
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
          rv = prefBranch->GetCharPref("port", getter_Copies(prefString));
          if (NS_SUCCEEDED(rv) && !prefString.IsEmpty()) {
            parameters.Append(" ");
            parameters.Append(prefString);
          }
        }
        /* username */
        if (!uUsername.IsEmpty()) {
          rv = prefBranch->GetCharPref("username", getter_Copies(prefString));
          if (NS_SUCCEEDED(rv) && !prefString.IsEmpty()) {
            parameters.Append(" ");
            parameters.Append(prefString);
          }
        }
        /* password */
        if (!uPassword.IsEmpty()) {
          rv = prefBranch->GetCharPref("password", getter_Copies(prefString));
          if (NS_SUCCEEDED(rv) && !prefString.IsEmpty()) {
            parameters.Append(" ");
            parameters.Append(prefString);
          }
        }
        /* host */
        if (!uHost.IsEmpty()) {
          rv = prefBranch->GetCharPref("host", getter_Copies(prefString));
          if (NS_SUCCEEDED(rv) && !prefString.IsEmpty()) {
            parameters.Append(" ");
            parameters.Append(prefString);
          }
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

  PRUint32 pid;
  if (NS_FAILED(rv = process->Run(PR_FALSE, params, numParams, &pid)))
    return rv;

  return NS_OK;
}

