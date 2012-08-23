/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This class allows you to prefix an existing nsIAsyncInputStream
 * with a preloaded block of data known at construction time by wrapping the
 * two data sources into a new nsIAsyncInputStream. Readers of the new
 * stream initially see the preloaded data and when that has been exhausted
 * they automatically read from the wrapped stream.
 *
 * It is used by nsHttpConnection when it has over buffered while reading from
 * the HTTP input socket and accidentally consumed data that belongs to
 * a different protocol via the HTTP Upgrade mechanism. That over-buffered
 * data is preloaded together with the input socket to form the new input socket
 * given to the new protocol handler.
*/

#ifndef nsPreloadedStream_h__
#define nsPreloadedStream_h__

#include "nsIAsyncInputStream.h"
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace net {

class nsPreloadedStream MOZ_FINAL : public nsIAsyncInputStream
{
 public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINPUTSTREAM
    NS_DECL_NSIASYNCINPUTSTREAM

    nsPreloadedStream(nsIAsyncInputStream *aStream, 
                      const char *data, uint32_t datalen);
private:
    ~nsPreloadedStream();

    nsCOMPtr<nsIAsyncInputStream> mStream;

    char *mBuf;
    uint32_t mOffset;
    uint32_t mLen;
};
        
} // namespace net
} // namespace mozilla

#endif
