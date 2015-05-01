/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef STREAM_H_
#define STREAM_H_

#include "nsISupportsImpl.h"

namespace mp4_demuxer
{

class Stream
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Stream);

  virtual bool ReadAt(int64_t offset, void* data, size_t size,
                      size_t* bytes_read) = 0;
  virtual bool CachedReadAt(int64_t offset, void* data, size_t size,
                            size_t* bytes_read) = 0;
  virtual bool Length(int64_t* size) = 0;

  virtual void DiscardBefore(int64_t offset) {}

protected:
  virtual ~Stream() {}
};

}

#endif
