/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionPort.h"
#include "ExtensionEventManager.h"

#include "mozilla/dom/BindingUtils.h"  // SequenceRooter
#include "mozilla/dom/ExtensionPortBinding.h"
#include "mozilla/dom/ScriptSettings.h"  // AutoEntryScript
#include "nsIGlobalObject.h"

namespace mozilla {
namespace extensions {

NS_IMPL_CYCLE_COLLECTING_ADDREF(ExtensionPort);
NS_IMPL_CYCLE_COLLECTING_RELEASE(ExtensionPort)
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ExtensionPort, mGlobal,
                                      mOnDisconnectEventMgr,
                                      mOnMessageEventMgr);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ExtensionPort)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// static
already_AddRefed<ExtensionPort> ExtensionPort::Create(
    nsIGlobalObject* aGlobal, JS::Handle<JS::Value> aDescriptorValue,
    ErrorResult& aRv) {
  if (!aDescriptorValue.isObject()) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  dom::AutoEntryScript aes(&aDescriptorValue.toObject(), __func__);
  JSContext* acx = aes.cx();
  auto portDescriptor = MakeUnique<dom::ExtensionPortDescriptor>();
  if (!portDescriptor->Init(acx, aDescriptorValue, __func__)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  RefPtr<ExtensionPort> port =
      new ExtensionPort(aGlobal, std::move(portDescriptor));
  return port.forget();
}

ExtensionPort::ExtensionPort(
    nsIGlobalObject* aGlobal,
    UniquePtr<dom::ExtensionPortDescriptor> aPortDescriptor)
    : mGlobal(aGlobal), mPortDescriptor(std::move(aPortDescriptor)) {
  MOZ_DIAGNOSTIC_ASSERT(mGlobal);
}

nsString ExtensionPort::GetAPIObjectId() const {
  return mPortDescriptor->mPortId;
}

JSObject* ExtensionPort::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return dom::ExtensionPort_Binding::Wrap(aCx, this, aGivenProto);
}

nsIGlobalObject* ExtensionPort::GetParentObject() const { return mGlobal; }

ExtensionEventManager* ExtensionPort::OnMessage() {
  if (!mOnMessageEventMgr) {
    mOnMessageEventMgr = CreateEventManager(u"onMessage"_ns);
  }

  return mOnMessageEventMgr;
}

ExtensionEventManager* ExtensionPort::OnDisconnect() {
  if (!mOnDisconnectEventMgr) {
    mOnDisconnectEventMgr = CreateEventManager(u"onDisconnect"_ns);
  }

  return mOnDisconnectEventMgr;
}

void ExtensionPort::GetName(nsAString& aString) {
  aString.Assign(mPortDescriptor->mName);
}

}  // namespace extensions
}  // namespace mozilla
