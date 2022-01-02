/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef _nsDeflateConverter_h_
#define _nsDeflateConverter_h_

#include "nsIStreamConverter.h"
#include "nsCOMPtr.h"
#include "zlib.h"
#include "mozilla/Attributes.h"

#define DEFLATECONVERTER_CID                         \
  {                                                  \
    0x461cd5dd, 0x73c6, 0x47a4, {                    \
      0x8c, 0xc3, 0x60, 0x3b, 0x37, 0xd8, 0x4a, 0x61 \
    }                                                \
  }

class nsDeflateConverter final : public nsIStreamConverter {
 public:
  static constexpr size_t kZipBufLen = (4 * 1024 - 1);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSISTREAMCONVERTER

  nsDeflateConverter() : mWrapMode(WRAP_NONE), mOffset(0), mZstream() {
    // 6 is Z_DEFAULT_COMPRESSION but we need the actual value
    mLevel = 6;
  }

  explicit nsDeflateConverter(int32_t level)
      : mWrapMode(WRAP_NONE), mOffset(0), mZstream() {
    mLevel = level;
  }

 private:
  ~nsDeflateConverter() {}

  enum WrapMode { WRAP_ZLIB, WRAP_GZIP, WRAP_NONE };

  WrapMode mWrapMode;
  uint64_t mOffset;
  int32_t mLevel;
  nsCOMPtr<nsIStreamListener> mListener;
  nsCOMPtr<nsISupports> mContext;
  z_stream mZstream;
  unsigned char mWriteBuffer[kZipBufLen];

  nsresult Init();
  nsresult PushAvailableData(nsIRequest* aRequest);
};

#endif
