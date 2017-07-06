/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsStartupCacheUtils_h_
#define nsStartupCacheUtils_h_

#include "nsString.h"
#include "nsIStorageStream.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "mozilla/UniquePtr.h"

class nsIURI;

namespace mozilla {
namespace scache {

nsresult
NewObjectInputStreamFromBuffer(UniquePtr<char[]> buffer, uint32_t len,
                               nsIObjectInputStream** stream);

// We can't retrieve the wrapped stream from the objectOutputStream later,
// so we return it here. We give callers in debug builds the option
// to wrap the outputstream in a debug stream, which will detect if
// non-singleton objects are written out multiple times during a serialization.
// This could cause them to be deserialized incorrectly (as multiple copies
// instead of references).
nsresult
NewObjectOutputWrappedStorageStream(nsIObjectOutputStream **wrapperStream,
                                    nsIStorageStream** stream,
                                    bool wantDebugStream);

// Creates a buffer for storing the stream into the cache. The buffer is
// allocated with 'new []'.  After calling this function, the caller would
// typically call nsIStartupCache::PutBuffer with the returned buffer.
nsresult
NewBufferFromStorageStream(nsIStorageStream *storageStream,
                           UniquePtr<char[]>* buffer, uint32_t* len);

nsresult
PathifyURI(nsIURI *in, nsACString &out);
} // namespace scache
} // namespace mozilla

#endif //nsStartupCacheUtils_h_
