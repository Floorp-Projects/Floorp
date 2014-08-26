/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BUFFER_STREAM_H_
#define BUFFER_STREAM_H_

#include "mp4_demuxer/mp4_demuxer.h"

namespace mp4_demuxer {

class BufferStream : public Stream
{
public:
  /* BufferStream does not take ownership of aData nor does it make a copy.
   * Therefore BufferStream shouldn't get used after aData is destroyed.
   */
  BufferStream(const uint8_t* aData, size_t aLength);

  virtual bool ReadAt(int64_t aOffset, void* aData, size_t aLength,
                      size_t* aBytesRead) MOZ_OVERRIDE;
  virtual bool Length(int64_t* aLength) MOZ_OVERRIDE;

private:
  const uint8_t* mData;
  size_t mLength;
};
}

#endif
