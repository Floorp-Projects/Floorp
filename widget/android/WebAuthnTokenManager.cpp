/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AndroidWebAuthnTokenManager.h"

namespace mozilla {
class WebAuthnTokenManager final
    : public java::WebAuthnTokenManager::Natives<WebAuthnTokenManager> {
 public:
  static void WebAuthnMakeCredentialFinish(
      jni::ByteArray::Param aClientDataJson, jni::ByteArray::Param aKeyHandle,
      jni::ByteArray::Param aAttestationObject) {
    mozilla::dom::AndroidWebAuthnResult result;

    result.mClientDataJSON.Assign(
        reinterpret_cast<const char*>(
            aClientDataJson->GetElements().Elements()),
        aClientDataJson->Length());
    result.mKeyHandle.Assign(
        reinterpret_cast<uint8_t*>(aKeyHandle->GetElements().Elements()),
        aKeyHandle->Length());
    result.mAttObj.Assign(reinterpret_cast<uint8_t*>(
                              aAttestationObject->GetElements().Elements()),
                          aAttestationObject->Length());

    mozilla::dom::AndroidWebAuthnTokenManager::GetInstance()
        ->HandleRegisterResult(std::move(result));
  }

  static void WebAuthnMakeCredentialReturnError(jni::String::Param aErrorCode) {
    mozilla::dom::AndroidWebAuthnResult result(aErrorCode->ToString());
    mozilla::dom::AndroidWebAuthnTokenManager::GetInstance()
        ->HandleRegisterResult(std::move(result));
  }

  static void WebAuthnGetAssertionFinish(jni::ByteArray::Param aClientDataJson,
                                         jni::ByteArray::Param aKeyHandle,
                                         jni::ByteArray::Param aAuthData,
                                         jni::ByteArray::Param aSignature,
                                         jni::ByteArray::Param aUserHandle) {
    mozilla::dom::AndroidWebAuthnResult result;

    result.mClientDataJSON.Assign(
        reinterpret_cast<const char*>(
            aClientDataJson->GetElements().Elements()),
        aClientDataJson->Length());
    result.mKeyHandle.Assign(
        reinterpret_cast<uint8_t*>(aKeyHandle->GetElements().Elements()),
        aKeyHandle->Length());
    result.mAuthData.Assign(
        reinterpret_cast<uint8_t*>(aAuthData->GetElements().Elements()),
        aAuthData->Length());
    result.mSignature.Assign(
        reinterpret_cast<uint8_t*>(aSignature->GetElements().Elements()),
        aSignature->Length());
    result.mUserHandle.Assign(
        reinterpret_cast<uint8_t*>(aUserHandle->GetElements().Elements()),
        aUserHandle->Length());

    mozilla::dom::AndroidWebAuthnTokenManager::GetInstance()->HandleSignResult(
        std::move(result));
  }

  static void WebAuthnGetAssertionReturnError(jni::String::Param aErrorCode) {
    mozilla::dom::AndroidWebAuthnResult result(aErrorCode->ToString());
    mozilla::dom::AndroidWebAuthnTokenManager::GetInstance()->HandleSignResult(
        std::move(result));
  }
};
}  // namespace mozilla
