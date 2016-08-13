/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozilla_widget_GfxInfo_h__
#define __mozilla_widget_GfxInfo_h__

#include "GfxInfoBase.h"

namespace mozilla {
namespace widget {

class GfxInfo : public GfxInfoBase
{
  ~GfxInfo() {}
public:
  GfxInfo();

  // We only declare the subset of nsIGfxInfo that we actually implement. The
  // rest is brought forward from GfxInfoBase.
  NS_IMETHOD GetD2DEnabled(bool *aD2DEnabled) override;
  NS_IMETHOD GetDWriteEnabled(bool *aDWriteEnabled) override;
  NS_IMETHOD GetDWriteVersion(nsAString & aDwriteVersion) override;
  NS_IMETHOD GetCleartypeParameters(nsAString & aCleartypeParams) override;
  NS_IMETHOD GetAdapterDescription(nsAString & aAdapterDescription) override;
  NS_IMETHOD GetAdapterDriver(nsAString & aAdapterDriver) override;
  NS_IMETHOD GetAdapterVendorID(nsAString & aAdapterVendorID) override;
  NS_IMETHOD GetAdapterDeviceID(nsAString & aAdapterDeviceID) override;
  NS_IMETHOD GetAdapterSubsysID(nsAString & aAdapterSubsysID) override;
  NS_IMETHOD GetAdapterRAM(nsAString & aAdapterRAM) override;
  NS_IMETHOD GetAdapterDriverVersion(nsAString & aAdapterDriverVersion) override;
  NS_IMETHOD GetAdapterDriverDate(nsAString & aAdapterDriverDate) override;
  NS_IMETHOD GetAdapterDescription2(nsAString & aAdapterDescription) override;
  NS_IMETHOD GetAdapterDriver2(nsAString & aAdapterDriver) override;
  NS_IMETHOD GetAdapterVendorID2(nsAString & aAdapterVendorID) override;
  NS_IMETHOD GetAdapterDeviceID2(nsAString & aAdapterDeviceID) override;
  NS_IMETHOD GetAdapterSubsysID2(nsAString & aAdapterSubsysID) override;
  NS_IMETHOD GetAdapterRAM2(nsAString & aAdapterRAM) override;
  NS_IMETHOD GetAdapterDriverVersion2(nsAString & aAdapterDriverVersion) override;
  NS_IMETHOD GetAdapterDriverDate2(nsAString & aAdapterDriverDate) override;
  NS_IMETHOD GetIsGPU2Active(bool *aIsGPU2Active) override;
  using GfxInfoBase::GetFeatureStatus;
  using GfxInfoBase::GetFeatureSuggestedDriverVersion;
  using GfxInfoBase::GetWebGLParameter;

  virtual nsresult Init() override;

  virtual uint32_t OperatingSystemVersion() override { return mWindowsVersion; }

  nsresult FindMonitors(JSContext* cx, JS::HandleObject array) override;

#ifdef DEBUG
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIGFXINFODEBUG
#endif

protected:

  virtual nsresult GetFeatureStatusImpl(int32_t aFeature,
                                        int32_t *aStatus,
                                        nsAString & aSuggestedDriverVersion,
                                        const nsTArray<GfxDriverInfo>& aDriverInfo,
                                        nsACString& aFailureId,
                                        OperatingSystem* aOS = nullptr) override;
  virtual const nsTArray<GfxDriverInfo>& GetGfxDriverInfo() override;

  void DescribeFeatures(JSContext* cx, JS::Handle<JSObject*> aOut) override;

private:

  void AddCrashReportAnnotations();

  nsString mDeviceString;
  nsString mDeviceID;
  nsString mDriverVersion;
  nsString mDriverDate;
  nsString mDeviceKey;
  nsString mDeviceKeyDebug;
  nsString mAdapterVendorID;
  nsString mAdapterDeviceID;
  nsString mAdapterSubsysID;
  nsString mDeviceString2;
  nsString mDriverVersion2;
  nsString mDeviceID2;
  nsString mDriverDate2;
  nsString mDeviceKey2;
  nsString mAdapterVendorID2;
  nsString mAdapterDeviceID2;
  nsString mAdapterSubsysID2;
  uint32_t mWindowsVersion;
  bool mHasDualGPU;
  bool mIsGPU2Active;
  bool mHasDriverVersionMismatch;
};

} // namespace widget
} // namespace mozilla

#endif /* __mozilla_widget_GfxInfo_h__ */
