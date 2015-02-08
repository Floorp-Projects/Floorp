/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozilla_widget_GfxInfoBase_h__
#define __mozilla_widget_GfxInfoBase_h__

#include "nsIGfxInfo.h"
#if defined(XP_MACOSX) || defined(XP_WIN)
#include "nsIGfxInfo2.h"
#endif
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "GfxDriverInfo.h"
#include "nsTArray.h"
#include "nsString.h"
#include "GfxInfoCollector.h"
#include "nsIGfxInfoDebug.h"
#include "mozilla/Mutex.h"
#include "js/Value.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace widget {  

class GfxInfoBase : public nsIGfxInfo,
#if defined(XP_MACOSX) || defined(XP_WIN)
                    public nsIGfxInfo2,
#endif
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
  NS_IMETHOD GetFeatureStatus(int32_t aFeature, int32_t *_retval) MOZ_OVERRIDE;
  NS_IMETHOD GetFeatureSuggestedDriverVersion(int32_t aFeature, nsAString & _retval) MOZ_OVERRIDE;
  NS_IMETHOD GetWebGLParameter(const nsAString & aParam, nsAString & _retval) MOZ_OVERRIDE;

    NS_IMETHOD GetFailures(uint32_t *failureCount, int32_t** indices, char ***failures) MOZ_OVERRIDE;
  NS_IMETHOD_(void) LogFailure(const nsACString &failure) MOZ_OVERRIDE;
  NS_IMETHOD GetInfo(JSContext*, JS::MutableHandle<JS::Value>) MOZ_OVERRIDE;

  // Initialization function. If you override this, you must call this class's
  // version of Init first.
  // We need Init to be called separately from the constructor so we can
  // register as an observer after all derived classes have been constructed
  // and we know we have a non-zero refcount.
  // Ideally, Init() would be void-return, but the rules of
  // NS_GENERIC_FACTORY_CONSTRUCTOR_INIT require it be nsresult return.
  virtual nsresult Init();
  
  // only useful on X11
  NS_IMETHOD_(void) GetData() MOZ_OVERRIDE { }

  static void AddCollector(GfxInfoCollectorBase* collector);
  static void RemoveCollector(GfxInfoCollectorBase* collector);

  static nsTArray<GfxDriverInfo>* mDriverInfo;
  static bool mDriverInfoObserverInitialized;

  virtual nsString Model() { return EmptyString(); }
  virtual nsString Hardware() { return EmptyString(); }
  virtual nsString Product() { return EmptyString(); }
  virtual nsString Manufacturer() { return EmptyString(); }
  virtual uint32_t OperatingSystemVersion() { return 0; }

protected:

  virtual ~GfxInfoBase();

  virtual nsresult GetFeatureStatusImpl(int32_t aFeature, int32_t* aStatus,
                                        nsAString& aSuggestedDriverVersion,
                                        const nsTArray<GfxDriverInfo>& aDriverInfo,
                                        OperatingSystem* aOS = nullptr);

  // Gets the driver info table. Used by GfxInfoBase to check for general cases
  // (while subclasses check for more specific ones).
  virtual const nsTArray<GfxDriverInfo>& GetGfxDriverInfo() = 0;

private:
  virtual int32_t FindBlocklistedDeviceInList(const nsTArray<GfxDriverInfo>& aDriverInfo,
                                              nsAString& aSuggestedVersion,
                                              int32_t aFeature,
                                              OperatingSystem os);

  void EvaluateDownloadedBlacklist(nsTArray<GfxDriverInfo>& aDriverInfo);

  Mutex mMutex;

};

}
}

#endif /* __mozilla_widget_GfxInfoBase_h__ */
