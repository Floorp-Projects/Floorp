/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
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

#include "GfxInfo.h"
#include "GfxInfoWebGL.h"
#include "nsUnicharUtils.h"
#include "nsPrintfCString.h"
#include "mozilla/FunctionTimer.h"
#include "prenv.h"
#include "prprf.h"
#include "EGLUtils.h"

#include "AndroidBridge.h"

#if defined(MOZ_CRASHREPORTER) && defined(MOZ_ENABLE_LIBXUL)
#include "nsExceptionHandler.h"
#include "nsICrashReporter.h"
#define NS_CRASHREPORTER_CONTRACTID "@mozilla.org/toolkit/crash-reporter;1"
#include "nsIPrefService.h"
#endif


using namespace mozilla::widget;


NS_IMPL_ISUPPORTS1(GfxInfo, nsIGfxInfo)

/* GetD2DEnabled and GetDwriteEnabled shouldn't be called until after gfxPlatform initialization
 * has occurred because they depend on it for information. (See bug 591561) */
nsresult
GfxInfo::GetD2DEnabled(PRBool *aEnabled)
{
  return NS_ERROR_FAILURE;
}

nsresult
GfxInfo::GetDWriteEnabled(PRBool *aEnabled)
{
  return NS_ERROR_FAILURE;
}

void
GfxInfo::Init()
{
}


/* readonly attribute DOMString adapterDescription; */
NS_IMETHODIMP
GfxInfo::GetAdapterDescription(nsAString & aAdapterDescription)
{
  aAdapterDescription.AssignASCII(mozilla::gl::GetVendor());
  if (mozilla::AndroidBridge::Bridge()) {
      nsAutoString str;
      aAdapterDescription.Append(NS_LITERAL_STRING(" "));
      if (mozilla::AndroidBridge::Bridge()->GetStaticStringField("android/os/Build", "MODEL", str))
        aAdapterDescription.Append(str);
      aAdapterDescription.Append(NS_LITERAL_STRING(" "));
      if (mozilla::AndroidBridge::Bridge()->GetStaticStringField("android/os/Build", "MANUFACTURER", str))
        aAdapterDescription.Append(str);
      aAdapterDescription.Append(NS_LITERAL_STRING(" "));
      if (mozilla::AndroidBridge::Bridge()->GetStaticStringField("android/os/Build", "HARDWARE", str))
        aAdapterDescription.Append(str);
  }

  return NS_OK;
}

/* readonly attribute DOMString adapterRAM; */
NS_IMETHODIMP
GfxInfo::GetAdapterRAM(nsAString & aAdapterRAM)
{
  aAdapterRAM.AssignLiteral("");
  return NS_OK;
}

/* readonly attribute DOMString adapterDriver; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriver(nsAString & aAdapterDriver)
{
  aAdapterDriver.AssignLiteral("");
  return NS_OK;
}

/* readonly attribute DOMString adapterDriverVersion; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverVersion(nsAString & aAdapterDriverVersion)
{
  aAdapterDriverVersion.AssignLiteral("");
  return NS_OK;
}

/* readonly attribute DOMString adapterDriverDate; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverDate(nsAString & aAdapterDriverDate)
{
  aAdapterDriverDate.AssignLiteral("");
  return NS_OK;
}

/* readonly attribute unsigned long adapterVendorID; */
NS_IMETHODIMP
GfxInfo::GetAdapterVendorID(PRUint32 *aAdapterVendorID)
{
  *aAdapterVendorID = 0;
  return NS_OK;
}

/* readonly attribute unsigned long adapterDeviceID; */
NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID(PRUint32 *aAdapterDeviceID)
{
  *aAdapterDeviceID = 0;
  return NS_OK;
}

void
GfxInfo::AddCrashReportAnnotations()
{
#if 0
#if defined(MOZ_CRASHREPORTER) && defined(MOZ_ENABLE_LIBXUL)
  nsCAutoString deviceIDString, vendorIDString;
  PRUint32 deviceID, vendorID;

  GetAdapterDeviceID(&deviceID);
  GetAdapterVendorID(&vendorID);

  deviceIDString.AppendPrintf("%04x", deviceID);
  vendorIDString.AppendPrintf("%04x", vendorID);

  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("AdapterVendorID"),
      vendorIDString);
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("AdapterDeviceID"),
      deviceIDString);

  /* Add an App Note for now so that we get the data immediately. These
   * can go away after we store the above in the socorro db */
  nsCAutoString note;
  /* AppendPrintf only supports 32 character strings, mrghh. */
  note.AppendPrintf("AdapterVendorID: %04x, ", vendorID);
  note.AppendPrintf("AdapterDeviceID: %04x", deviceID);

  if (vendorID == 0) {
      /* if we didn't find a valid vendorID lets append the mDeviceID string to try to find out why */
      note.Append(", ");
      note.AppendWithConversion(mDeviceID);
      note.Append(", ");
      note.AppendWithConversion(mDeviceKeyDebug);
  }
  note.Append("\n");

  CrashReporter::AppendAppNotesToCrashReport(note);

#endif
#endif
}

NS_IMETHODIMP
GfxInfo::GetFeatureStatus(PRInt32 aFeature, PRInt32 *aStatus)
{
  PRInt32 status = nsIGfxInfo::FEATURE_NO_INFO;

  if (aFeature == FEATURE_OPENGL_LAYERS) {
      nsAutoString str;
      /* Whitelist Galaxy S phones */
      if (mozilla::AndroidBridge::Bridge()->GetStaticStringField("android/os/Build", "HARDWARE", str)) {
          if (str != NS_LITERAL_STRING("samsung smdkc110")) {
            status = FEATURE_BLOCKED_DEVICE;
          }
      }
  }

  *aStatus = status;
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetFeatureSuggestedDriverVersion(PRInt32 aFeature, nsAString& aSuggestedDriverVersion)
{
  return NS_OK;
}

NS_IMETHODIMP
GfxInfo::GetWebGLParameter(const nsAString& aParam, nsAString& aResult)
{
  return GfxInfoWebGL::GetWebGLParameter(aParam, aResult);
}
