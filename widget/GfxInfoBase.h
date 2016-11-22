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
                  , public nsIGfxInfoDebug
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
  // using GfxInfoBase::GetWebGLParameter;
  // to import the relevant methods into their namespace.
  NS_IMETHOD GetFeatureStatus(int32_t aFeature, nsACString& aFailureId, int32_t *_retval) override;
  NS_IMETHOD GetFeatureSuggestedDriverVersion(int32_t aFeature, nsAString & _retval) override;
  NS_IMETHOD GetWebGLParameter(const nsAString & aParam, nsAString & _retval) override;

  NS_IMETHOD GetMonitors(JSContext* cx, JS::MutableHandleValue _retval) override;
  NS_IMETHOD GetFailures(uint32_t *failureCount, int32_t** indices, char ***failures) override;
  NS_IMETHOD_(void) LogFailure(const nsACString &failure) override;
  NS_IMETHOD GetInfo(JSContext*, JS::MutableHandle<JS::Value>) override;
  NS_IMETHOD GetFeatures(JSContext*, JS::MutableHandle<JS::Value>) override;
  NS_IMETHOD GetFeatureLog(JSContext*, JS::MutableHandle<JS::Value>) override;
  NS_IMETHOD GetActiveCrashGuards(JSContext*, JS::MutableHandle<JS::Value>) override;
  NS_IMETHOD GetContentBackend(nsAString & aContentBackend) override;
  NS_IMETHOD GetUsingGPUProcess(bool *aOutValue) override;

  // Initialization function. If you override this, you must call this class's
  // version of Init first.
  // We need Init to be called separately from the constructor so we can
  // register as an observer after all derived classes have been constructed
  // and we know we have a non-zero refcount.
  // Ideally, Init() would be void-return, but the rules of
  // NS_GENERIC_FACTORY_CONSTRUCTOR_INIT require it be nsresult return.
  virtual nsresult Init();
  
  // only useful on X11
  NS_IMETHOD_(void) GetData() override { }

  static void AddCollector(GfxInfoCollectorBase* collector);
  static void RemoveCollector(GfxInfoCollectorBase* collector);

  static nsTArray<GfxDriverInfo>* mDriverInfo;
  static bool mDriverInfoObserverInitialized;
  static bool mShutdownOccurred;

  virtual nsString Model() { return EmptyString(); }
  virtual nsString Hardware() { return EmptyString(); }
  virtual nsString Product() { return EmptyString(); }
  virtual nsString Manufacturer() { return EmptyString(); }
  virtual uint32_t OperatingSystemVersion() { return 0; }

  // Convenience to get the application version
  static const nsCString& GetApplicationVersion();

  virtual nsresult FindMonitors(JSContext* cx, JS::HandleObject array) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

protected:

  virtual ~GfxInfoBase();

  virtual nsresult GetFeatureStatusImpl(int32_t aFeature, int32_t* aStatus,
                                        nsAString& aSuggestedDriverVersion,
                                        const nsTArray<GfxDriverInfo>& aDriverInfo,
                                        nsACString& aFailureId,
                                        OperatingSystem* aOS = nullptr);

  // Gets the driver info table. Used by GfxInfoBase to check for general cases
  // (while subclasses check for more specific ones).
  virtual const nsTArray<GfxDriverInfo>& GetGfxDriverInfo() = 0;

  virtual void DescribeFeatures(JSContext* aCx, JS::Handle<JSObject*> obj);
  bool InitFeatureObject(
    JSContext* aCx,
    JS::Handle<JSObject*> aContainer,
    const char* aName,
    int32_t aFeature,
    Maybe<mozilla::gfx::FeatureStatus> aKnownStatus,
    JS::MutableHandle<JSObject*> aOutObj);

private:
  virtual int32_t FindBlocklistedDeviceInList(const nsTArray<GfxDriverInfo>& aDriverInfo,
                                              nsAString& aSuggestedVersion,
                                              int32_t aFeature,
                                              nsACString &aFailureId,
                                              OperatingSystem os);

  void EvaluateDownloadedBlacklist(nsTArray<GfxDriverInfo>& aDriverInfo);

  bool BuildFeatureStateLog(JSContext* aCx, const gfx::FeatureState& aFeature,
                            JS::MutableHandle<JS::Value> aOut);

  Mutex mMutex;

};

} // namespace widget
} // namespace mozilla

#endif /* __mozilla_widget_GfxInfoBase_h__ */
