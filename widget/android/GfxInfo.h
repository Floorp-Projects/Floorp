/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozilla_widget_GfxInfo_h__
#define __mozilla_widget_GfxInfo_h__

#include "GfxInfoBase.h"
#include "GfxDriverInfo.h"

#include "nsString.h"
#include "mozilla/Scoped.h"

namespace mozilla {

namespace gl {
class GLContext;
}

namespace widget {

class GfxInfo : public GfxInfoBase
{
public:
  GfxInfo();

  // We only declare the subset of nsIGfxInfo that we actually implement. The
  // rest is brought forward from GfxInfoBase.
  NS_IMETHOD GetD2DEnabled(bool *aD2DEnabled);
  NS_IMETHOD GetDWriteEnabled(bool *aDWriteEnabled);
  NS_IMETHOD GetDWriteVersion(nsAString & aDwriteVersion);
  NS_IMETHOD GetCleartypeParameters(nsAString & aCleartypeParams);
  NS_IMETHOD GetAdapterDescription(nsAString & aAdapterDescription);
  NS_IMETHOD GetAdapterDriver(nsAString & aAdapterDriver);
  NS_IMETHOD GetAdapterVendorID(nsAString & aAdapterVendorID);
  NS_IMETHOD GetAdapterDeviceID(nsAString & aAdapterDeviceID);
  NS_IMETHOD GetAdapterSubsysID(nsAString & aAdapterSubsysID);
  NS_IMETHOD GetAdapterRAM(nsAString & aAdapterRAM);
  NS_IMETHOD GetAdapterDriverVersion(nsAString & aAdapterDriverVersion);
  NS_IMETHOD GetAdapterDriverDate(nsAString & aAdapterDriverDate);
  NS_IMETHOD GetAdapterDescription2(nsAString & aAdapterDescription);
  NS_IMETHOD GetAdapterDriver2(nsAString & aAdapterDriver);
  NS_IMETHOD GetAdapterVendorID2(nsAString & aAdapterVendorID);
  NS_IMETHOD GetAdapterDeviceID2(nsAString & aAdapterDeviceID);
  NS_IMETHOD GetAdapterSubsysID2(nsAString & aAdapterSubsysID);
  NS_IMETHOD GetAdapterRAM2(nsAString & aAdapterRAM);
  NS_IMETHOD GetAdapterDriverVersion2(nsAString & aAdapterDriverVersion);
  NS_IMETHOD GetAdapterDriverDate2(nsAString & aAdapterDriverDate);
  NS_IMETHOD GetIsGPU2Active(bool *aIsGPU2Active);
  using GfxInfoBase::GetFeatureStatus;
  using GfxInfoBase::GetFeatureSuggestedDriverVersion;
  using GfxInfoBase::GetWebGLParameter;

  void EnsureInitialized();

  virtual nsString Model();
  virtual nsString Hardware();
  virtual nsString Product();
  virtual nsString Manufacturer();

#ifdef DEBUG
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIGFXINFODEBUG
#endif

  virtual uint32_t OperatingSystemVersion() MOZ_OVERRIDE;

protected:

  virtual nsresult GetFeatureStatusImpl(int32_t aFeature, 
                                        int32_t *aStatus, 
                                        nsAString & aSuggestedDriverVersion, 
                                        const nsTArray<GfxDriverInfo>& aDriverInfo,
                                        OperatingSystem* aOS = nullptr);
  virtual const nsTArray<GfxDriverInfo>& GetGfxDriverInfo();

private:

  void AddCrashReportAnnotations();

  bool mInitialized;

  class GLStrings;
  ScopedDeletePtr<GLStrings> mGLStrings;

  nsCString mAdapterDescription;

  OperatingSystem mOS;

  nsString mModel, mHardware, mManufacturer, mProduct;
  nsCString mOSVersion;
  uint32_t mOSVersionInteger;
};

} // namespace widget
} // namespace mozilla

#endif /* __mozilla_widget_GfxInfo_h__ */
