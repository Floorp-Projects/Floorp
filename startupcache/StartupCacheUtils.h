/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsStartupCacheUtils_h_
#define nsStartupCacheUtils_h_

#include "nsIStorageStream.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"

namespace mozilla {
namespace scache {

NS_EXPORT nsresult
NewObjectInputStreamFromBuffer(char* buffer, PRUint32 len, 
                               nsIObjectInputStream** stream);

// We can't retrieve the wrapped stream from the objectOutputStream later,
// so we return it here. We give callers in debug builds the option 
// to wrap the outputstream in a debug stream, which will detect if
// non-singleton objects are written out multiple times during a serialization.
// This could cause them to be deserialized incorrectly (as multiple copies
// instead of references).
NS_EXPORT nsresult
NewObjectOutputWrappedStorageStream(nsIObjectOutputStream **wrapperStream,
                                    nsIStorageStream** stream,
                                    bool wantDebugStream);

// Creates a buffer for storing the stream into the cache. The buffer is
// allocated with 'new []'. Typically, the caller would store the buffer in
// an nsAutoArrayPtr<char> and then call nsIStartupCache::PutBuffer with it.
NS_EXPORT nsresult
NewBufferFromStorageStream(nsIStorageStream *storageStream, 
                           char** buffer, PRUint32* len);

NS_EXPORT nsresult
PathifyURI(nsIURI *in, nsACString &out);
}
}
#endif //nsStartupCacheUtils_h_
