/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozilla_widget_GfxInfoBase_h__
#define __mozilla_widget_GfxInfoBase_h__

#include "GfxDriverInfo.h"
#include "GfxInfoCollector.h"
#include "gfxFeature.h"
#include "gfxTelemetry.h"
#include "js/Value.h"
#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/gfx/GraphicsMessages.h"
#include "nsCOMPtr.h"
#include "nsIGfxInfo.h"
#include "nsIGfxInfoDebug.h"
#include "nsIObserver.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWeakReference.h"

namespace mozilla {
namespace widget {

class GfxInfoBase : public nsIGfxInfo,
                    public nsIObserver,
                    public nsSupportsWeakReference
#ifdef DEBUG
    ,
                    public nsIGfxInfoDebug
#endif
{
 public:
  GfxInfoBase();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  // We only declare a subset of the nsIGfxInfo interface. It's up to derived
  // classes to implement the rest of the interface.
  // Derived classes need to use
  // using GfxInfoBase::GetFeatureStatus;
  // using GfxInfoBase::GetFeatureSuggestedDriverVersion;
  // to import the relevant methods into their namespace.
  NS_IMETHOD GetFeatureStatus(int32_t aFeature, nsACString& aFailureId,
                              int32_t* _retval) override;
  NS_IMETHOD GetFeatureSuggestedDriverVersion(int32_t aFeature,
                                              nsAString& _retval) override;

  NS_IMETHOD GetMonitors(JSContext* cx,
                         JS::MutableHandleValue _retval) override;
  NS_IMETHOD RefreshMonitors() override;
  NS_IMETHOD GetFailures(nsTArray<int32_t>& indices,
                         nsTArray<nsCString>& failures) override;
  NS_IMETHOD_(void) LogFailure(const nsACString& failure) override;
  NS_IMETHOD GetInfo(JSContext*, JS::MutableHandle<JS::Value>) override;
  NS_IMETHOD GetFeatures(JSContext*, JS::MutableHandle<JS::Value>) override;
  NS_IMETHOD GetFeatureLog(JSContext*, JS::MutableHandle<JS::Value>) override;
  NS_IMETHOD GetActiveCrashGuards(JSContext*,
                                  JS::MutableHandle<JS::Value>) override;
  NS_IMETHOD GetContentBackend(nsAString& aContentBackend) override;
  NS_IMETHOD GetAzureCanvasBackend(nsAString& aBackend) override;
  NS_IMETHOD GetAzureContentBackend(nsAString& aBackend) override;
  NS_IMETHOD GetUsingGPUProcess(bool* aOutValue) override;
  NS_IMETHOD GetWebRenderEnabled(bool* aWebRenderEnabled) override;
  NS_IMETHOD GetIsHeadless(bool* aIsHeadless) override;
  NS_IMETHOD GetTargetFrameRate(uint32_t* aTargetFrameRate) override;

  // Non-XPCOM method to get IPC data:
  nsTArray<mozilla::gfx::GfxInfoFeatureStatus> GetAllFeatures();

  // Initialization function. If you override this, you must call this class's
  // version of Init first.
  // We need Init to be called separately from the constructor so we can
  // register as an observer after all derived classes have been constructed
  // and we know we have a non-zero refcount.
  // Ideally, Init() would be void-return, but the rules of
  // NS_GENERIC_FACTORY_CONSTRUCTOR_INIT require it be nsresult return.
  virtual nsresult Init();

  NS_IMETHOD_(void) GetData() override;
  NS_IMETHOD_(int32_t) GetMaxRefreshRate(bool* aMixed) override {
    if (aMixed) {
      *aMixed = false;
    }
    return -1;
  }

  static void AddCollector(GfxInfoCollectorBase* collector);
  static void RemoveCollector(GfxInfoCollectorBase* collector);

  static nsTArray<GfxDriverInfo>* sDriverInfo;
  static StaticAutoPtr<nsTArray<mozilla::gfx::GfxInfoFeatureStatus>>
      sFeatureStatus;
  static bool sDriverInfoObserverInitialized;
  static bool sShutdownOccurred;

  virtual nsString Model() { return u""_ns; }
  virtual nsString Hardware() { return u""_ns; }
  virtual nsString Product() { return u""_ns; }
  virtual nsString Manufacturer() { return u""_ns; }
  virtual uint32_t OperatingSystemVersion() { return 0; }
  virtual uint32_t OperatingSystemBuild() { return 0; }

  // Convenience to get the application version
  static const nsCString& GetApplicationVersion();

  virtual nsresult FindMonitors(JSContext* cx, JS::HandleObject array);

  static void SetFeatureStatus(
      nsTArray<mozilla::gfx::GfxInfoFeatureStatus>&& aFS);

 protected:
  virtual ~GfxInfoBase();

  virtual nsresult GetFeatureStatusImpl(
      int32_t aFeature, int32_t* aStatus, nsAString& aSuggestedDriverVersion,
      const nsTArray<GfxDriverInfo>& aDriverInfo, nsACString& aFailureId,
      OperatingSystem* aOS = nullptr);

  // Gets the driver info table. Used by GfxInfoBase to check for general cases
  // (while subclasses check for more specific ones).
  virtual const nsTArray<GfxDriverInfo>& GetGfxDriverInfo() = 0;

  virtual void DescribeFeatures(JSContext* aCx, JS::Handle<JSObject*> obj);

  bool DoesDesktopEnvironmentMatch(const nsAString& aBlocklistDesktop,
                                   const nsAString& aDesktopEnv);

  virtual bool DoesWindowProtocolMatch(
      const nsAString& aBlocklistWindowProtocol,
      const nsAString& aWindowProtocol);

  bool DoesVendorMatch(const nsAString& aBlocklistVendor,
                       const nsAString& aAdapterVendor);

  virtual bool DoesDriverVendorMatch(const nsAString& aBlocklistVendor,
                                     const nsAString& aDriverVendor);

  bool InitFeatureObject(JSContext* aCx, JS::Handle<JSObject*> aContainer,
                         const char* aName,
                         mozilla::gfx::FeatureState& aFeatureState,
                         JS::MutableHandle<JSObject*> aOutObj);

  NS_IMETHOD ControlGPUProcessForXPCShell(bool aEnable, bool* _retval) override;

  // Total number of pixels for all detected screens at startup.
  int64_t mScreenPixels;

 private:
  virtual int32_t FindBlocklistedDeviceInList(
      const nsTArray<GfxDriverInfo>& aDriverInfo, nsAString& aSuggestedVersion,
      int32_t aFeature, nsACString& aFailureId, OperatingSystem os,
      bool aForAllowing);

  bool IsFeatureAllowlisted(int32_t aFeature) const;

  void EvaluateDownloadedBlocklist(nsTArray<GfxDriverInfo>& aDriverInfo);

  bool BuildFeatureStateLog(JSContext* aCx, const gfx::FeatureState& aFeature,
                            JS::MutableHandle<JS::Value> aOut);

  Mutex mMutex;
};

}  // namespace widget
}  // namespace mozilla

#endif /* __mozilla_widget_GfxInfoBase_h__ */
