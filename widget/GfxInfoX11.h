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

class GfxInfo MOZ_FINAL : public GfxInfoBase
{
public:

  // We only declare the subset of nsIGfxInfo that we actually implement. The
  // rest is brought forward from GfxInfoBase.
  NS_IMETHOD GetD2DEnabled(bool *aD2DEnabled) MOZ_OVERRIDE;
  NS_IMETHOD GetDWriteEnabled(bool *aDWriteEnabled) MOZ_OVERRIDE;
  NS_IMETHOD GetDWriteVersion(nsAString & aDwriteVersion) MOZ_OVERRIDE;
  NS_IMETHOD GetCleartypeParameters(nsAString & aCleartypeParams) MOZ_OVERRIDE;
  NS_IMETHOD GetAdapterDescription(nsAString & aAdapterDescription) MOZ_OVERRIDE;
  NS_IMETHOD GetAdapterDriver(nsAString & aAdapterDriver) MOZ_OVERRIDE;
  NS_IMETHOD GetAdapterVendorID(nsAString & aAdapterVendorID) MOZ_OVERRIDE;
  NS_IMETHOD GetAdapterDeviceID(nsAString & aAdapterDeviceID) MOZ_OVERRIDE;
  NS_IMETHOD GetAdapterSubsysID(nsAString & aAdapterSubsysID) MOZ_OVERRIDE;
  NS_IMETHOD GetAdapterRAM(nsAString & aAdapterRAM) MOZ_OVERRIDE;
  NS_IMETHOD GetAdapterDriverVersion(nsAString & aAdapterDriverVersion) MOZ_OVERRIDE;
  NS_IMETHOD GetAdapterDriverDate(nsAString & aAdapterDriverDate) MOZ_OVERRIDE;
  NS_IMETHOD GetAdapterDescription2(nsAString & aAdapterDescription) MOZ_OVERRIDE;
  NS_IMETHOD GetAdapterDriver2(nsAString & aAdapterDriver) MOZ_OVERRIDE;
  NS_IMETHOD GetAdapterVendorID2(nsAString & aAdapterVendorID) MOZ_OVERRIDE;
  NS_IMETHOD GetAdapterDeviceID2(nsAString & aAdapterDeviceID) MOZ_OVERRIDE;
  NS_IMETHOD GetAdapterSubsysID2(nsAString & aAdapterSubsysID) MOZ_OVERRIDE;
  NS_IMETHOD GetAdapterRAM2(nsAString & aAdapterRAM) MOZ_OVERRIDE;
  NS_IMETHOD GetAdapterDriverVersion2(nsAString & aAdapterDriverVersion) MOZ_OVERRIDE;
  NS_IMETHOD GetAdapterDriverDate2(nsAString & aAdapterDriverDate) MOZ_OVERRIDE;
  NS_IMETHOD GetIsGPU2Active(bool *aIsGPU2Active) MOZ_OVERRIDE;
  using GfxInfoBase::GetFeatureStatus;
  using GfxInfoBase::GetFeatureSuggestedDriverVersion;
  using GfxInfoBase::GetWebGLParameter;

  virtual nsresult Init() MOZ_OVERRIDE;
  
  NS_IMETHOD_(void) GetData() MOZ_OVERRIDE;

#ifdef DEBUG
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIGFXINFODEBUG
#endif

protected:
  ~GfxInfo() {}

  virtual nsresult GetFeatureStatusImpl(int32_t aFeature, 
                                        int32_t *aStatus, 
                                        nsAString & aSuggestedDriverVersion, 
                                        const nsTArray<GfxDriverInfo>& aDriverInfo, 
                                        OperatingSystem* aOS = nullptr) MOZ_OVERRIDE;
  virtual const nsTArray<GfxDriverInfo>& GetGfxDriverInfo() MOZ_OVERRIDE;

private:
  nsCString mVendor;
  nsCString mRenderer;
  nsCString mVersion;
  nsCString mAdapterDescription;
  nsCString mOS;
  nsCString mOSRelease;
  bool mIsMesa, mIsNVIDIA, mIsFGLRX, mIsNouveau, mIsIntel, mIsOldSwrast, mIsLlvmpipe;
  bool mHasTextureFromPixmap;
  int mGLMajorVersion, mMajorVersion, mMinorVersion, mRevisionVersion;

  void AddCrashReportAnnotations();
};

} // namespace widget
} // namespace mozilla

#endif /* __GfxInfoX11_h__ */
