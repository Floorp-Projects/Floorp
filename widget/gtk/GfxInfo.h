/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WIDGET_GTK_GFXINFO_h__
#define WIDGET_GTK_GFXINFO_h__

#include "GfxInfoBase.h"
#include "nsString.h"

namespace mozilla {
namespace widget {

class GfxInfo final : public GfxInfoBase {
 public:
  // We only declare the subset of nsIGfxInfo that we actually implement. The
  // rest is brought forward from GfxInfoBase.
  NS_IMETHOD GetD2DEnabled(bool* aD2DEnabled) override;
  NS_IMETHOD GetDWriteEnabled(bool* aDWriteEnabled) override;
  NS_IMETHOD GetDWriteVersion(nsAString& aDwriteVersion) override;
  NS_IMETHOD GetEmbeddedInFirefoxReality(
      bool* aEmbeddedInFirefoxReality) override;
  NS_IMETHOD GetHasBattery(bool* aHasBattery) override;
  NS_IMETHOD GetCleartypeParameters(nsAString& aCleartypeParams) override;
  NS_IMETHOD GetWindowProtocol(nsAString& aWindowProtocol) override;
  NS_IMETHOD GetDesktopEnvironment(nsAString& aDesktopEnvironment) override;
  NS_IMETHOD GetTestType(nsAString& aTestType) override;
  NS_IMETHOD GetAdapterDescription(nsAString& aAdapterDescription) override;
  NS_IMETHOD GetAdapterDriver(nsAString& aAdapterDriver) override;
  NS_IMETHOD GetAdapterVendorID(nsAString& aAdapterVendorID) override;
  NS_IMETHOD GetAdapterDeviceID(nsAString& aAdapterDeviceID) override;
  NS_IMETHOD GetAdapterSubsysID(nsAString& aAdapterSubsysID) override;
  NS_IMETHOD GetAdapterRAM(uint32_t* aAdapterRAM) override;
  NS_IMETHOD GetAdapterDriverVendor(nsAString& aAdapterDriverVendor) override;
  NS_IMETHOD GetAdapterDriverVersion(nsAString& aAdapterDriverVersion) override;
  NS_IMETHOD GetAdapterDriverDate(nsAString& aAdapterDriverDate) override;
  NS_IMETHOD GetAdapterDescription2(nsAString& aAdapterDescription) override;
  NS_IMETHOD GetAdapterDriver2(nsAString& aAdapterDriver) override;
  NS_IMETHOD GetAdapterVendorID2(nsAString& aAdapterVendorID) override;
  NS_IMETHOD GetAdapterDeviceID2(nsAString& aAdapterDeviceID) override;
  NS_IMETHOD GetAdapterSubsysID2(nsAString& aAdapterSubsysID) override;
  NS_IMETHOD GetAdapterRAM2(uint32_t* aAdapterRAM) override;
  NS_IMETHOD GetAdapterDriverVendor2(nsAString& aAdapterDriverVendor) override;
  NS_IMETHOD GetAdapterDriverVersion2(
      nsAString& aAdapterDriverVersion) override;
  NS_IMETHOD GetAdapterDriverDate2(nsAString& aAdapterDriverDate) override;
  NS_IMETHOD GetIsGPU2Active(bool* aIsGPU2Active) override;
  NS_IMETHOD GetDisplayInfo(nsTArray<nsString>& aDisplayInfo) override;
  NS_IMETHOD GetDisplayWidth(nsTArray<uint32_t>& aDisplayWidth) override;
  NS_IMETHOD GetDisplayHeight(nsTArray<uint32_t>& aDisplayHeight) override;
  NS_IMETHOD GetDrmRenderDevice(nsACString& aDrmRenderDevice) override;
  using GfxInfoBase::GetFeatureStatus;
  using GfxInfoBase::GetFeatureSuggestedDriverVersion;

  virtual nsresult Init() override;

  NS_IMETHOD_(void) GetData() override;

#ifdef DEBUG
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIGFXINFODEBUG
#endif

 protected:
  ~GfxInfo() = default;

  virtual nsresult GetFeatureStatusImpl(
      int32_t aFeature, int32_t* aStatus, nsAString& aSuggestedDriverVersion,
      const nsTArray<GfxDriverInfo>& aDriverInfo, nsACString& aFailureId,
      OperatingSystem* aOS = nullptr) override;
  virtual const nsTArray<GfxDriverInfo>& GetGfxDriverInfo() override;

  virtual bool DoesWindowProtocolMatch(
      const nsAString& aBlocklistWindowProtocol,
      const nsAString& aWindowProtocol) override;

  virtual bool DoesDriverVendorMatch(const nsAString& aBlocklistVendor,
                                     const nsAString& aDriverVendor) override;

 private:
  nsCString mVendorId;
  nsCString mDeviceId;
  nsCString mDriverVendor;
  nsCString mDriverVersion;
  nsCString mAdapterDescription;
  uint32_t mAdapterRAM;
  nsCString mOS;
  nsCString mOSRelease;
  nsAutoCStringN<16> mDesktopEnvironment;
  nsCString mTestType;

  nsCString mSecondaryVendorId;
  nsCString mSecondaryDeviceId;

  nsCString mDrmRenderDevice;

  struct ScreenInfo {
    uint32_t mWidth;
    uint32_t mHeight;
    bool mIsDefault;
  };

  nsTArray<ScreenInfo> mScreenInfo;
  bool mHasTextureFromPixmap;
  unsigned int mGLMajorVersion, mGLMinorVersion;
  bool mIsMesa;
  bool mIsAccelerated;
  bool mIsWayland;
  bool mIsXWayland;
  bool mHasMultipleGPUs;
  bool mGlxTestError;

  void AddCrashReportAnnotations();
};

}  // namespace widget
}  // namespace mozilla

#endif /* WIDGET_GTK_GFXINFO_h__ */
