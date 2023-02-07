/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionAlarms.h"
#include "ExtensionEventManager.h"

#include "mozilla/dom/ExtensionAlarmsBinding.h"
#include "nsIGlobalObject.h"

namespace mozilla {
namespace extensions {

NS_IMPL_CYCLE_COLLECTING_ADDREF(ExtensionAlarms);
NS_IMPL_CYCLE_COLLECTING_RELEASE(ExtensionAlarms)
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ExtensionAlarms, mGlobal,
                                      mExtensionBrowser, mOnAlarmEventMgr);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ExtensionAlarms)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_WEBEXT_EVENTMGR(ExtensionAlarms, u"onAlarm"_ns, OnAlarm)

ExtensionAlarms::ExtensionAlarms(nsIGlobalObject* aGlobal,
                                 ExtensionBrowser* aExtensionBrowser)
    : mGlobal(aGlobal), mExtensionBrowser(aExtensionBrowser) {
  MOZ_DIAGNOSTIC_ASSERT(mGlobal);
  MOZ_DIAGNOSTIC_ASSERT(mExtensionBrowser);
}

/* static */
bool ExtensionAlarms::IsAllowed(JSContext* aCx, JSObject* aGlobal) {
  // TODO(Bug 1725478): this API visibility should be gated by the "alarms"
  // permission.
  return true;
}

JSObject* ExtensionAlarms::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return dom::ExtensionAlarms_Binding::Wrap(aCx, this, aGivenProto);
}

nsIGlobalObject* ExtensionAlarms::GetParentObject() const { return mGlobal; }

}  // namespace extensions
}  // namespace mozilla
