/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_StreamFilter_h
#define mozilla_extensions_StreamFilter_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/StreamFilterBinding.h"

#include "mozilla/DOMEventTargetHelper.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIAtom.h"

namespace mozilla {
namespace ipc {
  template <class T> class Endpoint;
}

namespace extensions {

class PStreamFilterChild;
class StreamFilterChild;

using namespace mozilla::dom;

class StreamFilter : public DOMEventTargetHelper
{
  friend class StreamFilterChild;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(StreamFilter, DOMEventTargetHelper)

  static already_AddRefed<StreamFilter>
  Create(GlobalObject& global,
         uint64_t aRequestId,
         const nsAString& aAddonId);

  explicit StreamFilter(nsIGlobalObject* aParent,
                        uint64_t aRequestId,
                        const nsAString& aAddonId);

  IMPL_EVENT_HANDLER(start);
  IMPL_EVENT_HANDLER(stop);
  IMPL_EVENT_HANDLER(data);
  IMPL_EVENT_HANDLER(error);

  void Write(const ArrayBufferOrUint8Array& aData,
             ErrorResult& aRv);

  void GetError(nsAString& aError)
  {
    aError = mError;
  }

  StreamFilterStatus Status() const;
  void Suspend(ErrorResult& aRv);
  void Resume(ErrorResult& aRv);
  void Disconnect(ErrorResult& aRv);
  void Close(ErrorResult& aRv);

  nsISupports* GetParentObject() const { return mParent; }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  static bool
  IsAllowedInContext(JSContext* aCx, JSObject* aObj);

protected:
  virtual ~StreamFilter();

  void FireEvent(const nsAString& aType);

  void FireDataEvent(const nsTArray<uint8_t>& aData);

  void FireErrorEvent(const nsAString& aError);

private:
  void
  Connect();

  void
  FinishConnect(mozilla::ipc::Endpoint<PStreamFilterChild>&& aEndpoint);

  void ForgetActor();

  nsCOMPtr<nsIGlobalObject> mParent;
  RefPtr<StreamFilterChild> mActor;

  nsString mError;

  const uint64_t mChannelId;
  const nsCOMPtr<nsIAtom> mAddonId;
};

} // namespace extensions
} // namespace mozilla

#endif // mozilla_extensions_StreamFilter_h
