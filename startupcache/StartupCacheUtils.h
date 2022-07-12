/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

nsresult NewObjectInputStreamFromBuffer(const char* buffer, uint32_t len,
                                        nsIObjectInputStream** stream);

// We can't retrieve the wrapped stream from the objectOutputStream later,
// so we return it here. We give callers in debug builds the option
// to wrap the outputstream in a debug stream, which will detect if
// non-singleton objects are written out multiple times during a serialization.
// This could cause them to be deserialized incorrectly (as multiple copies
// instead of references).
nsresult NewObjectOutputWrappedStorageStream(
    nsIObjectOutputStream** wrapperStream, nsIStorageStream** stream,
    bool wantDebugStream);

// Creates a buffer for storing the stream into the cache. The buffer is
// allocated with 'new []'.  After calling this function, the caller would
// typically call StartupCache::PutBuffer with the returned buffer.
nsresult NewBufferFromStorageStream(nsIStorageStream* storageStream,
                                    UniquePtr<char[]>* buffer, uint32_t* len);

nsresult ResolveURI(nsIURI* in, nsIURI** out);

// PathifyURI transforms uris into useful zip paths
// to make it easier to manipulate startup cache entries
// using standard zip tools.
//
// Transformations applied:
//  * resource:// URIs are resolved to their corresponding file/jar URI to
//    canonicalize resources URIs other than gre and app.
//  * Paths under GRE or APP directory have their base path replaced with
//    resource/gre or resource/app to avoid depending on install location.
//  * jar:file:///path/to/file.jar!/sub/path urls are replaced with
//    /path/to/file.jar/sub/path
//
// The result is concatenated with loaderType and stored into the string
// passed in.
//
// For example, in the js loader (loaderType = "jsloader"):
//  resource://gre/modules/XPCOMUtils.sys.mjs or
//  file://$GRE_DIR/modules/XPCOMUtils.sys.mjs or
//  jar:file://$GRE_DIR/omni.jar!/modules/XPCOMUtils.sys.mjs becomes
//     jsloader/resource/gre/modules/XPCOMUtils.sys.mjs
//  file://$PROFILE_DIR/extensions/{uuid}/components/component.js becomes
//     jsloader/$PROFILE_DIR/extensions/%7Buuid%7D/components/component.js
//  jar:file://$PROFILE_DIR/extensions/some.xpi!/components/component.js becomes
//     jsloader/$PROFILE_DIR/extensions/some.xpi/components/component.js
nsresult PathifyURI(const char* loaderType, size_t loaderTypeLength, nsIURI* in,
                    nsACString& out);

template <int N>
nsresult PathifyURI(const char (&loaderType)[N], nsIURI* in, nsACString& out) {
  return PathifyURI(loaderType, N - 1, in, out);
}

}  // namespace scache
}  // namespace mozilla

#endif  // nsStartupCacheUtils_h_
