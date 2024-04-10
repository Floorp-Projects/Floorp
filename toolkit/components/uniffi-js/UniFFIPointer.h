/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_UniFFIPointer_h
#define mozilla_dom_UniFFIPointer_h

#include "nsISupports.h"
#include "nsWrapperCache.h"
#include "nsString.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/UniFFIPointerType.h"

namespace mozilla::dom {

class UniFFIPointer final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(UniFFIPointer)

  static already_AddRefed<UniFFIPointer> Create(
      void* aPtr, const uniffi::UniFFIPointerType* aType);
  static already_AddRefed<UniFFIPointer> Read(
      const ArrayBuffer& aArrayBuff, uint32_t aPosition,
      const uniffi::UniFFIPointerType* aType, ErrorResult& aError);
  void Write(const ArrayBuffer& aArrayBuff, uint32_t aPosition,
             const uniffi::UniFFIPointerType* aType, ErrorResult& aError) const;

  UniFFIPointer(void* aPtr, const uniffi::UniFFIPointerType* aType);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
  nsISupports* GetParentObject() { return nullptr; }

  /**
   * Clone the raw pointer that `UniFFIPointer` holds
   *
   * Use this when lowering the pointer to pass it across the FFI, for example:
   *   - When calling a method
   *   - When passing the object as an argument to a function
   */
  void* ClonePtr() const;

  /**
   * Returns true if the pointer type `this` holds is the same as the argument
   * it does so using pointer comparison, as there is **exactly** one static
   * `UniFFIPointerType` per type exposed in the UniFFI interface
   */
  bool IsSamePtrType(const uniffi::UniFFIPointerType* type) const;

 private:
  const uniffi::UniFFIPointerType* mType;
  void* mPtr;

 protected:
  /**
   * Destructs the `UniFFIPointer`, making sure to give back ownership of the
   * raw pointer back to Rust, which deallocates the pointer
   */
  ~UniFFIPointer();
};
}  // namespace mozilla::dom

#endif /* mozilla_dom_UniFFIPointer_h */
