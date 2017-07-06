/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsJARProtocolHandler_h__
#define nsJARProtocolHandler_h__

#include "nsIJARProtocolHandler.h"
#include "nsIProtocolHandler.h"
#include "nsIJARURI.h"
#include "nsIZipReader.h"
#include "nsIMIMEService.h"
#include "nsWeakReference.h"
#include "nsCOMPtr.h"

class nsJARProtocolHandler final : public nsIJARProtocolHandler
                                 , public nsSupportsWeakReference
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIPROTOCOLHANDLER
    NS_DECL_NSIJARPROTOCOLHANDLER

    // nsJARProtocolHandler methods:
    nsJARProtocolHandler();

    static nsJARProtocolHandler *GetSingleton();

    nsresult Init();

    // returns non addref'ed pointer.
    nsIMIMEService    *MimeService();
    nsIZipReaderCache *JarCache() { return mJARCache; }
protected:
    virtual ~nsJARProtocolHandler();

    nsCOMPtr<nsIZipReaderCache> mJARCache;
    nsCOMPtr<nsIMIMEService> mMimeService;
};

extern nsJARProtocolHandler *gJarHandler;

#define NS_JARPROTOCOLHANDLER_CID                    \
{ /* 0xc7e410d4-0x85f2-11d3-9f63-006008a6efe9 */     \
    0xc7e410d4,                                      \
    0x85f2,                                          \
    0x11d3,                                          \
    {0x9f, 0x63, 0x00, 0x60, 0x08, 0xa6, 0xef, 0xe9} \
}

#endif // !nsJARProtocolHandler_h__
