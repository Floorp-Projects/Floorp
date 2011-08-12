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
#include "nsToolkit.h"

#if defined(MOZ_CRASHREPORTER)
#include "nsExceptionHandler.h"
#include "nsICrashReporter.h"
#define NS_CRASHREPORTER_CONTRACTID "@mozilla.org/toolkit/crash-reporter;1"
#endif

using namespace mozilla::widget;

nsresult
GfxInfo::Init()
{
  NS_TIME_FUNCTION;

  nsresult rv = GfxInfoBase::Init();

  CGLRendererInfoObj renderer = 0;
  GLint rendererCount = 0;

  memset(mRendererIDs, 0, sizeof(mRendererIDs));

  if (CGLQueryRendererInfo(0xffffffff, &renderer, &rendererCount) != kCGLNoError)
    return rv;

  rendererCount = (GLint) NS_MIN(rendererCount, (GLint) NS_ARRAY_LENGTH(mRendererIDs));
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
      mRendererIDs[i] = 0;
      mRendererIDsString.AppendPrintf("???");
    }
  }

  CGLDestroyRendererInfo(renderer);

  AddCrashReportAnnotations();

  return rv;
}

NS_IMETHODIMP
GfxInfo::GetD2DEnabled(PRBool *aEnabled)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetAzureEnabled(PRBool *aEnabled)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GfxInfo::GetDWriteEnabled(PRBool *aEnabled)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString DWriteVersion; */
NS_IMETHODIMP
GfxInfo::GetDWriteVersion(nsAString & aDwriteVersion)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString cleartypeParameters; */
NS_IMETHODIMP
GfxInfo::GetCleartypeParameters(nsAString & aCleartypeParams)
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

/* readonly attribute DOMString adapterDescription2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDescription2(nsAString & aAdapterDescription)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterRAM; */
NS_IMETHODIMP
GfxInfo::GetAdapterRAM(nsAString & aAdapterRAM)
{
  aAdapterRAM = mAdapterRAMString;
  return NS_OK;
}

/* readonly attribute DOMString adapterRAM2; */
NS_IMETHODIMP
GfxInfo::GetAdapterRAM2(nsAString & aAdapterRAM)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterDriver; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriver(nsAString & aAdapterDriver)
{
  aAdapterDriver.AssignLiteral("");
  return NS_OK;
}

/* readonly attribute DOMString adapterDriver2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriver2(nsAString & aAdapterDriver)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterDriverVersion; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverVersion(nsAString & aAdapterDriverVersion)
{
  aAdapterDriverVersion.AssignLiteral("");
  return NS_OK;
}

/* readonly attribute DOMString adapterDriverVersion2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverVersion2(nsAString & aAdapterDriverVersion)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute DOMString adapterDriverDate; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverDate(nsAString & aAdapterDriverDate)
{
  aAdapterDriverDate.AssignLiteral("");
  return NS_OK;
}

/* readonly attribute DOMString adapterDriverDate2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDriverDate2(nsAString & aAdapterDriverDate)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute unsigned long adapterVendorID; */
NS_IMETHODIMP
GfxInfo::GetAdapterVendorID(PRUint32 *aAdapterVendorID)
{
  *aAdapterVendorID = 0;
  return NS_OK;
}

/* readonly attribute unsigned long adapterVendorID2; */
NS_IMETHODIMP
GfxInfo::GetAdapterVendorID2(PRUint32 *aAdapterVendorID)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute unsigned long adapterDeviceID; */
NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID(PRUint32 *aAdapterDeviceID)
{
  *aAdapterDeviceID = 0;
  return NS_OK;
}

/* readonly attribute unsigned long adapterDeviceID2; */
NS_IMETHODIMP
GfxInfo::GetAdapterDeviceID2(PRUint32 *aAdapterDeviceID)
{
  return NS_ERROR_FAILURE;
}

/* readonly attribute boolean isGPU2Active; */
NS_IMETHODIMP
GfxInfo::GetIsGPU2Active(PRBool* aIsGPU2Active)
{
  return NS_ERROR_FAILURE;
}

void
GfxInfo::AddCrashReportAnnotations()
{
#if defined(MOZ_CRASHREPORTER)
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

nsresult
GfxInfo::GetFeatureStatusImpl(PRInt32 aFeature, PRInt32* aStatus,
                              nsAString& aSuggestedDriverVersion,
                              GfxDriverInfo* aDriverInfo /* = nsnull */)
{
  NS_ENSURE_ARG_POINTER(aStatus);

  aSuggestedDriverVersion.SetIsVoid(PR_TRUE);

  PRInt32 status = nsIGfxInfo::FEATURE_NO_INFO;

  // For now, we don't implement the downloaded blacklist.
  if (aDriverInfo) {
    *aStatus = status;
    return NS_OK;
  }

  // Many WebGL issues on 10.5, especially:
  //   * bug 631258: WebGL shader paints using textures belonging to other processes on Mac OS 10.5
  //   * bug 618848: Post process shaders and texture mapping crash OS X 10.5
  if (aFeature == nsIGfxInfo::FEATURE_WEBGL_OPENGL &&
      !nsToolkit::OnSnowLeopardOrLater())
  {
    status = nsIGfxInfo::FEATURE_BLOCKED_OS_VERSION;
  }

  if (aFeature == nsIGfxInfo::FEATURE_OPENGL_LAYERS) {
    // CGL reports a list of renderers, some renderers are slow (e.g. software)
    // and AFAIK we can't decide which one will be used among them, so let's implement this by returning NO_INFO
    // if any not-known-to-be-bad renderer is found.
    // The assumption that we make here is that the system will spontaneously use the best/fastest renderer in the list.
    // Note that the presence of software renderer fallbacks means that slow software rendering may be automatically
    // used, which seems to be the case in bug 611292 where the user had a Intel GMA 945 card (non programmable hardware).
    // Therefore we need to explicitly blacklist non-OpenGL2 hardware, which could result in a software renderer
    // being used.
    PRBool foundGoodDevice = PR_FALSE;

    for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(mRendererIDs); ++i) {
      switch (mRendererIDs[i]) {
        case kCGLRendererATIRage128ID: // non-programmable
        case kCGLRendererATIRadeonID: // non-programmable
        case kCGLRendererATIRageProID: // non-programmable
        case kCGLRendererATIRadeon8500ID: // no OpenGL 2 support, http://en.wikipedia.org/wiki/Radeon_R200
        case kCGLRendererATIRadeon9700ID: // no OpenGL 2 support, http://en.wikipedia.org/wiki/Radeon_R200
        case kCGLRendererATIRadeonX1000ID: // can't render to non-power-of-two texture backed framebuffers
        case kCGLRendererIntel900ID: // non-programmable
        case kCGLRendererGeForce2MXID: // non-programmable
        case kCGLRendererGeForce3ID: // no OpenGL 2 support,
                                     // http://en.wikipedia.org/wiki/Comparison_of_Nvidia_graphics_processing_units
        case kCGLRendererGeForceFXID: // incomplete OpenGL 2 support with software fallbacks,
                                      // http://en.wikipedia.org/wiki/Comparison_of_Nvidia_graphics_processing_units
        case kCGLRendererVTBladeXP2ID: // Trident DX8 chip, assuming it's not GL2 capable
        case kCGLRendererMesa3DFXID: // non-programmable
        case kCGLRendererGenericFloatID: // software renderer
        case kCGLRendererGenericID: // software renderer
        case kCGLRendererAppleSWID: // software renderer
          break;
        default:
          if (mRendererIDs[i])
            foundGoodDevice = PR_TRUE;
      }
    }
    if (!foundGoodDevice)
      status = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
  }

  if (aFeature == nsIGfxInfo::FEATURE_WEBGL_OPENGL) {
    // same comment as above for FEATURE_OPENGL_LAYERS.
    bool foundGoodDevice = false;

    for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(mRendererIDs); ++i) {
      switch (mRendererIDs[i]) {
        case kCGLRendererGeForceFXID: // bug 678053. We must blacklist Geforce 7300 GT. This family
                                      // covers all Geforce FX, 6, 7 series. Need bug 678330 for finer
                                      // blacklisting.
          break;
        default:
          if (mRendererIDs[i])
            foundGoodDevice = true;
      }
    }
    if (!foundGoodDevice)
      status = nsIGfxInfo::FEATURE_BLOCKED_DEVICE;
  }

  *aStatus = status;
  return NS_OK;
}
