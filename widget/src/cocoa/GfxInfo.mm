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
 * Contributor(s):
 *   Jonathan Griffin <jgriffin@mozilla.com>
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

#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLRenderers.h>

#include "GfxInfo.h"
#include "nsUnicharUtils.h"
#include "mozilla/FunctionTimer.h"

#if defined(MOZ_CRASHREPORTER) && defined(MOZ_ENABLE_LIBXUL)
#include "nsExceptionHandler.h"
#include "nsICrashReporter.h"
#define NS_CRASHREPORTER_CONTRACTID "@mozilla.org/toolkit/crash-reporter;1"
#include "nsIPrefService.h"
#endif


using namespace mozilla::widget;

NS_IMPL_ISUPPORTS1(GfxInfo, nsIGfxInfo)

void
GfxInfo::Init()
{
  NS_TIME_FUNCTION;

  CGLRendererInfoObj renderer = 0;
  GLint rendererCount = 0;

  memset(mRendererIDs, 0, sizeof(mRendererIDs));

  if (CGLQueryRendererInfo(0xffffffff, &renderer, &rendererCount) != kCGLNoError)
    return;

  rendererCount = (GLint) PR_MIN(rendererCount, (GLint) NS_ARRAY_LENGTH(mRendererIDs));
  for (GLint i = 0; i < rendererCount; i++) {
    GLint prop = 0;

    if (!mRendererIDsString.IsEmpty())
      mRendererIDsString.AppendLiteral(",");
    if (CGLDescribeRenderer(renderer, i, kCGLRPRendererID, &prop) == kCGLNoError) {
#ifdef kCGLRendererIDMatchingMask
      prop = prop & kCGLRendererIDMatchingMask;
#else
      prop = prop & 0x00FE7F00; // this is the mask token above, but it doesn't seem to exist everywhere?
#endif
      mRendererIDs[i] = prop;
      mRendererIDsString.AppendPrintf("0x%04x", prop);
    } else {
      mRendererIDs[i] = -1;
      mRendererIDsString.AppendPrintf("???");
    }
  }

  CGLDestroyRendererInfo(renderer);

  AddCrashReportAnnotations();
}

NS_IMETHODIMP
GfxInfo::GetD2DEnabled(PRBool *aEnabled)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetDWriteEnabled(PRBool *aEnabled)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterDescription; */
NS_IMETHODIMP
GfxInfo::GetAdapterDescription(nsAString & aAdapterDescription)
{
  aAdapterDescription = mRendererIDsString;
  return NS_OK;
}

/* readonly attribute DOMString adapterRAM; */
NS_IMETHODIMP
GfxInfo::GetAdapterRAM(nsAString & aAdapterRAM)
{
  aAdapterRAM = mAdapterRAMString;
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
  *aAdapterVendorID = mRendererIDs[0];
  return NS_OK;
}

/* readonly attribute unsigned long adapterDeviceID; */
NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID(PRUint32 *aAdapterDeviceID)
{
  *aAdapterDeviceID = mRendererIDs[0];
  return NS_OK;
}

void
GfxInfo::AddCrashReportAnnotations()
{
#if defined(MOZ_CRASHREPORTER) && defined(MOZ_ENABLE_LIBXUL)
  CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("AdapterRendererIDs"),
                                     NS_LossyConvertUTF16toASCII(mRendererIDsString));

  /* Add an App Note for now so that we get the data immediately. These
   * can go away after we store the above in the socorro db */
  nsCAutoString note;
  /* AppendPrintf only supports 32 character strings, mrghh. */
  note.AppendLiteral("Renderers: ");
  note.Append(NS_LossyConvertUTF16toASCII(mRendererIDsString));

  CrashReporter::AppendAppNotesToCrashReport(note);
#endif
}

NS_IMETHODIMP
GfxInfo::GetFeatureStatus(PRInt32 aFeature, PRInt32 *aStatus)
{
  PRInt32 status = nsIGfxInfo::FEATURE_STATUS_UNKNOWN;

  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(mRendererIDs); ++i) {
    PRUint32 r = mRendererIDs[i];

    if (aFeature == nsIGfxInfo::FEATURE_OPENGL_LAYERS) {
      if (r == kCGLRendererATIRadeonX1000ID)
        status = nsIGfxInfo::FEATURE_BLOCKED;
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
