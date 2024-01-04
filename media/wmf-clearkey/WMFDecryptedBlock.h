/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFDECRYPTEDBLOCK_H
#define DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFDECRYPTEDBLOCK_H

#include <vector>

#include "WMFClearKeyUtils.h"
#include "content_decryption_module.h"

namespace mozilla {

// This class is used to store the decrypted data. It will be allocated by
// `SessionManagerWrapper::Allocate`.
class WMFDecryptedBuffer : public cdm::Buffer {
 public:
  explicit WMFDecryptedBuffer(size_t aSize) {
    SetSize(aSize);
    LOG("WMFDecryptedBuffer ctor %p, sz=%u", this, Size());
  }
  ~WMFDecryptedBuffer() override {
    LOG("WMFDecryptedBuffer dtor %p, sz=%u", this, Size());
  }
  WMFDecryptedBuffer(const WMFDecryptedBuffer&) = delete;
  WMFDecryptedBuffer& operator=(const WMFDecryptedBuffer&) = delete;

  // This is not good, but that is how cdm::buffer works.
  void Destroy() override { delete this; }
  uint32_t Capacity() const override { return mBuffer.size(); }
  uint8_t* Data() override { return mBuffer.data(); }
  void SetSize(uint32_t aSize) override { return mBuffer.resize(aSize); }
  uint32_t Size() const override { return mBuffer.size(); }

 private:
  std::vector<uint8_t> mBuffer;
};

class WMFDecryptedBlock : public cdm::DecryptedBlock {
 public:
  WMFDecryptedBlock() : mBuffer(nullptr), mTimestamp(0) {
    LOG("WMFDecryptedBlock ctor %p", this);
  }
  ~WMFDecryptedBlock() override {
    LOG("WMFDecryptedBlock dtor %p", this);
    if (mBuffer) {
      mBuffer->Destroy();
      mBuffer = nullptr;
    }
  }
  void SetDecryptedBuffer(cdm::Buffer* aBuffer) override {
    LOG("WMFDecryptedBlock(%p)::SetDecryptedBuffer, buffer=%p", this, aBuffer);
    mBuffer = aBuffer;
  }
  cdm::Buffer* DecryptedBuffer() override { return mBuffer; }
  void SetTimestamp(int64_t aTimestamp) override { mTimestamp = aTimestamp; }
  int64_t Timestamp() const override { return mTimestamp; }

 private:
  cdm::Buffer* mBuffer;
  int64_t mTimestamp;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFDECRYPTEDBLOCK_H
