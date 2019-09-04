/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Base64UtilsSupport_h__
#define Base64UtilsSupport_h__

#include "GeneratedJNINatives.h"
#include "mozilla/Base64.h"

namespace mozilla {
namespace widget {

class Base64UtilsSupport final
    : public java::Base64Utils::Natives<Base64UtilsSupport> {
 public:
  static jni::ByteArray::LocalRef Decode(jni::String::Param data) {
    FallibleTArray<uint8_t> bytes;
    if (NS_FAILED(Base64URLDecode(
            data->ToCString(), Base64URLDecodePaddingPolicy::Ignore, bytes))) {
      return nullptr;
    }

    return jni::ByteArray::New((const signed char*)bytes.Elements(),
                               bytes.Length());
  }

  static jni::String::LocalRef Encode(jni::ByteArray::Param data) {
    nsTArray<int8_t> bytes = data->GetElements();
    nsCString result;
    if (NS_FAILED(
            Base64URLEncode(data->Length(), (const uint8_t*)bytes.Elements(),
                            Base64URLEncodePaddingPolicy::Omit, result))) {
      return nullptr;
    }
    return jni::StringParam(result);
  }
};

}  // namespace widget
}  // namespace mozilla

#endif  // Base64UtilsSupport_h__
