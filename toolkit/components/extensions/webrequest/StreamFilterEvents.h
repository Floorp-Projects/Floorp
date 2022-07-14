/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_StreamFilterEvents_h
#define mozilla_extensions_StreamFilterEvents_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/StreamFilterDataEventBinding.h"
#include "mozilla/extensions/StreamFilter.h"

#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/Event.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla::extensions {

class StreamFilterDataEvent : public dom::Event {
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(StreamFilterDataEvent,
                                                         Event)

  explicit StreamFilterDataEvent(dom::EventTarget* aEventTarget)
      : Event(aEventTarget, nullptr, nullptr) {
    mozilla::HoldJSObjects(this);
  }

  static already_AddRefed<StreamFilterDataEvent> Constructor(
      dom::EventTarget* aEventTarget, const nsAString& aType,
      const dom::StreamFilterDataEventInit& aParam);

  static already_AddRefed<StreamFilterDataEvent> Constructor(
      dom::GlobalObject& aGlobal, const nsAString& aType,
      const dom::StreamFilterDataEventInit& aParam) {
    nsCOMPtr<dom::EventTarget> target =
        do_QueryInterface(aGlobal.GetAsSupports());
    return Constructor(target, aType, aParam);
  }

  void GetData(JSContext* aCx, JS::MutableHandle<JSObject*> aResult) {
    aResult.set(mData);
  }

  virtual JSObject* WrapObjectInternal(
      JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

 protected:
  virtual ~StreamFilterDataEvent() { mozilla::DropJSObjects(this); }

 private:
  JS::Heap<JSObject*> mData;

  void SetData(const dom::ArrayBuffer& aData) { mData = aData.Obj(); }
};

}  // namespace mozilla::extensions

#endif  // mozilla_extensions_StreamFilterEvents_h
