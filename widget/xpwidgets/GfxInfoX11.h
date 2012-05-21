/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __GfxInfoX11_h__
#define __GfxInfoX11_h__

#include "GfxInfoBase.h"

namespace mozilla {
namespace widget {

class GfxInfo : public GfxInfoBase
{
public:

  // We only declare the subset of nsIGfxInfo that we actually implement. The
  // rest is brought forward from GfxInfoBase.
  NS_SCRIPTABLE NS_IMETHOD GetD2DEnabled(bool *aD2DEnabled);
  NS_SCRIPTABLE NS_IMETHOD GetDWriteEnabled(bool *aDWriteEnabled);
  NS_SCRIPTABLE NS_IMETHOD GetAzureEnabled(bool *aAzureEnabled);
  NS_SCRIPTABLE NS_IMETHOD GetDWriteVersion(nsAString & aDwriteVersion);
  NS_SCRIPTABLE NS_IMETHOD GetCleartypeParameters(nsAString & aCleartypeParams);
  NS_SCRIPTABLE NS_IMETHOD GetAdapterDescription(nsAString & aAdapterDescription);
  NS_SCRIPTABLE NS_IMETHOD GetAdapterDriver(nsAString & aAdapterDriver);
  NS_SCRIPTABLE NS_IMETHOD GetAdapterVendorID(nsAString & aAdapterVendorID);
  NS_SCRIPTABLE NS_IMETHOD GetAdapterDeviceID(nsAString & aAdapterDeviceID);
  NS_SCRIPTABLE NS_IMETHOD GetAdapterRAM(nsAString & aAdapterRAM);
  NS_SCRIPTABLE NS_IMETHOD GetAdapterDriverVersion(nsAString & aAdapterDriverVersion);
  NS_SCRIPTABLE NS_IMETHOD GetAdapterDriverDate(nsAString & aAdapterDriverDate);
  NS_SCRIPTABLE NS_IMETHOD GetAdapterDescription2(nsAString & aAdapterDescription);
  NS_SCRIPTABLE NS_IMETHOD GetAdapterDriver2(nsAString & aAdapterDriver);
  NS_SCRIPTABLE NS_IMETHOD GetAdapterVendorID2(nsAString & aAdapterVendorID);
  NS_SCRIPTABLE NS_IMETHOD GetAdapterDeviceID2(nsAString & aAdapterDeviceID);
  NS_SCRIPTABLE NS_IMETHOD GetAdapterRAM2(nsAString & aAdapterRAM);
  NS_SCRIPTABLE NS_IMETHOD GetAdapterDriverVersion2(nsAString & aAdapterDriverVersion);
  NS_SCRIPTABLE NS_IMETHOD GetAdapterDriverDate2(nsAString & aAdapterDriverDate);
  NS_SCRIPTABLE NS_IMETHOD GetIsGPU2Active(bool *aIsGPU2Active);
  using GfxInfoBase::GetFeatureStatus;
  using GfxInfoBase::GetFeatureSuggestedDriverVersion;
  using GfxInfoBase::GetWebGLParameter;

  virtual nsresult Init();
  
  NS_IMETHOD_(void) GetData();

#ifdef DEBUG
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIGFXINFODEBUG
#endif

protected:

  virtual nsresult GetFeatureStatusImpl(PRInt32 aFeature, 
                                        PRInt32 *aStatus, 
                                        nsAString & aSuggestedDriverVersion, 
                                        const nsTArray<GfxDriverInfo>& aDriverInfo, 
                                        OperatingSystem* aOS = nsnull);
  virtual const nsTArray<GfxDriverInfo>& GetGfxDriverInfo();

private:
  nsCString mVendor;
  nsCString mRenderer;
  nsCString mVersion;
  nsCString mAdapterDescription;
  nsCString mOS;
  nsCString mOSRelease;
  bool mIsMesa, mIsNVIDIA, mIsFGLRX, mIsNouveau;
  bool mHasTextureFromPixmap;
  int mGLMajorVersion, mMajorVersion, mMinorVersion, mRevisionVersion;

  void AddCrashReportAnnotations();
};

} // namespace widget
} // namespace mozilla

#endif /* __GfxInfoX11_h__ */
