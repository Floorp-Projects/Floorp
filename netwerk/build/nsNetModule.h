/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNetModule_h
#define nsNetModule_h

#include "nsID.h"

class nsISupports;

nsresult nsNetStartup();
void nsNetShutdown();

nsresult CreateNewStreamConvServiceFactory(const nsIID& aIID, void** aResult);
nsresult CreateNewMultiMixedConvFactory(const nsIID& aIID, void** aResult);
nsresult CreateNewTXTToHTMLConvFactory(const nsIID& aIID, void** aResult);
nsresult CreateNewHTTPCompressConvFactory(const nsIID& aIID, void** aResult);
nsresult CreateNewUnknownDecoderFactory(const nsIID& aIID, void** aResult);
nsresult CreateNewBinaryDetectorFactory(const nsIID& aIID, void** aResult);
nsresult nsLoadGroupConstructor(const nsIID& aIID, void** aResult);

extern nsresult net_NewIncrementalDownload(const nsIID&, void**);

namespace mozilla {
namespace net {
nsresult WebSocketChannelConstructor(const nsIID& aIID, void** aResult);
nsresult WebSocketSSLChannelConstructor(const nsIID& aIID, void** aResult);
}  // namespace net
}  // namespace mozilla

#endif
