/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/extensions/StreamFilterEvents.h"

namespace mozilla {
namespace extensions {

NS_IMPL_CYCLE_COLLECTION_CLASS(StreamFilterDataEvent)

NS_IMPL_ADDREF_INHERITED(StreamFilterDataEvent, Event)
NS_IMPL_RELEASE_INHERITED(StreamFilterDataEvent, Event)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(StreamFilterDataEvent, Event)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(StreamFilterDataEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mData)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(StreamFilterDataEvent, Event)
  tmp->mData = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(StreamFilterDataEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)


/* static */ already_AddRefed<StreamFilterDataEvent>
StreamFilterDataEvent::Constructor(EventTarget* aEventTarget,
                                   const nsAString& aType,
                                   const StreamFilterDataEventInit& aParam)
{
  RefPtr<StreamFilterDataEvent> event = new StreamFilterDataEvent(aEventTarget);

  bool trusted = event->Init(aEventTarget);
  event->InitEvent(aType, aParam.mBubbles, aParam.mCancelable);
  event->SetTrusted(trusted);
  event->SetComposed(aParam.mComposed);

  event->SetData(aParam.mData);

  return event.forget();
}

JSObject*
StreamFilterDataEvent::WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return StreamFilterDataEventBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace extensions
} // namespace mozilla
