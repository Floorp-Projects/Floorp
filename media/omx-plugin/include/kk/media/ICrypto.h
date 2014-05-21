/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <binder/IInterface.h>
#include <media/stagefright/foundation/ABase.h>
#include <media/hardware/CryptoAPI.h>

#ifndef ANDROID_ICRYPTO_H_

#define ANDROID_ICRYPTO_H_

namespace android {

struct AString;

struct ICrypto : public IInterface {
    DECLARE_META_INTERFACE(Crypto);

    virtual status_t initCheck() const = 0;

    virtual bool isCryptoSchemeSupported(const uint8_t uuid[16]) = 0;

    virtual status_t createPlugin(
            const uint8_t uuid[16], const void *data, size_t size) = 0;

    virtual status_t destroyPlugin() = 0;

    virtual bool requiresSecureDecoderComponent(
            const char *mime) const = 0;

    virtual ssize_t decrypt(
            bool secure,
            const uint8_t key[16],
            const uint8_t iv[16],
            CryptoPlugin::Mode mode,
            const void *srcPtr,
            const CryptoPlugin::SubSample *subSamples, size_t numSubSamples,
            void *dstPtr,
            AString *errorDetailMsg) = 0;

private:
    DISALLOW_EVIL_CONSTRUCTORS(ICrypto);
};

struct BnCrypto : public BnInterface<ICrypto> {
    virtual status_t onTransact(
            uint32_t code, const Parcel &data, Parcel *reply,
            uint32_t flags = 0);
};

}  // namespace android

#endif // ANDROID_ICRYPTO_H_

