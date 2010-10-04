/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
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
 * The Original Code is Mozilla Webapp code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Fabrice Desré <fabrice@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "AndroidBridge.h"
#include "nsCRTGlue.h"
#include "nsWebappsSupport.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS1(nsWebappsSupport, nsIWebappsSupport)

NS_IMETHODIMP
nsWebappsSupport::InstallApplication(const PRUnichar *aTitle, const PRUnichar *aURI, const PRUnichar *aIconURI, const PRUnichar *aIconData)
{
  ALOG("in nsWebappsSupport::InstallApplication()\n");
  AndroidBridge::AutoLocalJNIFrame jniFrame;
  JNIEnv *jEnv = GetJNIForThread();
  jclass jGeckoAppShellClass = GetGeckoAppShellClass();
  
  if (!jEnv || !jGeckoAppShellClass)
    return NS_ERROR_FAILURE;
    
  jmethodID jInstallWebApplication = jEnv->GetStaticMethodID(jGeckoAppShellClass, "installWebApplication", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
  jstring jstrURI = jEnv->NewString(aURI, NS_strlen(aURI));
  jstring jstrTitle = jEnv->NewString(aTitle, NS_strlen(aTitle));
  jstring jstrIconData = jEnv->NewString(aIconData, NS_strlen(aIconData));
  
  if (!jstrURI || !jstrTitle || !jstrIconData)
    return NS_ERROR_FAILURE;
    
  jEnv->CallStaticVoidMethod(jGeckoAppShellClass, jInstallWebApplication, jstrURI, jstrTitle, jstrIconData);
  return NS_OK;
}

/*
 * we have no way to know if an application is installed, so pretend it's not installed
 */
NS_IMETHODIMP
nsWebappsSupport::IsApplicationInstalled(const PRUnichar *aURI, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

