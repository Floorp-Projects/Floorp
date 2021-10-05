/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionTest.h"
#include "ExtensionEventManager.h"

#include "mozilla/dom/ExtensionTestBinding.h"
#include "nsIGlobalObject.h"

namespace mozilla {
namespace extensions {

bool IsInAutomation(JSContext* aCx, JSObject* aGlobal) {
  return NS_IsMainThread()
             ? xpc::IsInAutomation()
             : dom::WorkerGlobalScope::IsInAutomation(aCx, aGlobal);
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(ExtensionTest);
NS_IMPL_CYCLE_COLLECTING_RELEASE(ExtensionTest)
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ExtensionTest, mGlobal, mExtensionBrowser,
                                      mOnMessageEventMgr);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ExtensionTest)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

ExtensionTest::ExtensionTest(nsIGlobalObject* aGlobal,
                             ExtensionBrowser* aExtensionBrowser)
    : mGlobal(aGlobal), mExtensionBrowser(aExtensionBrowser) {
  MOZ_DIAGNOSTIC_ASSERT(mGlobal);
  MOZ_DIAGNOSTIC_ASSERT(mExtensionBrowser);
}

/* static */
bool ExtensionTest::IsAllowed(JSContext* aCx, JSObject* aGlobal) {
  // Allow browser.test API namespace while running in xpcshell tests.
  if (PR_GetEnv("XPCSHELL_TEST_PROFILE_DIR")) {
    return true;
  }

  return IsInAutomation(aCx, aGlobal);
}

JSObject* ExtensionTest::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return dom::ExtensionTest_Binding::Wrap(aCx, this, aGivenProto);
}

nsIGlobalObject* ExtensionTest::GetParentObject() const { return mGlobal; }

ExtensionEventManager* ExtensionTest::OnMessage() {
  if (!mOnMessageEventMgr) {
    mOnMessageEventMgr = CreateEventManager(u"onMessage"_ns);
  }

  return mOnMessageEventMgr;
}

}  // namespace extensions
}  // namespace mozilla
