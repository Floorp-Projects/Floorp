/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BUFFER_STREAM_H_
#define BUFFER_STREAM_H_

#include "mp4_demuxer/mp4_demuxer.h"
#include "nsTArray.h"
#include "ResourceQueue.h"

namespace mp4_demuxer {

class BufferStream : public Stream
{
public:
  /* BufferStream does not take ownership of aData nor does it make a copy.
   * Therefore BufferStream shouldn't get used after aData is destroyed.
   */
  BufferStream();

  bool ReadAt(int64_t aOffset, void* aData, size_t aLength,
              size_t* aBytesRead);
  bool CachedReadAt(int64_t aOffset, void* aData, size_t aLength,
                    size_t* aBytesRead);
  bool Length(int64_t* aLength);

  void DiscardBefore(int64_t aOffset);

  void AppendBytes(const uint8_t* aData, size_t aLength);
  void AppendData(mozilla::ResourceItem* aItem);

  mozilla::MediaByteRange GetByteRange();

  int64_t mStartOffset;
  int64_t mLogicalLength;
  int64_t mStartIndex;
  nsTArray<nsRefPtr<mozilla::ResourceItem>> mData;
};
}

#endif
