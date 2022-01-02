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

nsresult CreateNewStreamConvServiceFactory(nsISupports* aOuter,
                                           const nsIID& aIID, void** aResult);
nsresult CreateNewMultiMixedConvFactory(nsISupports* aOuter, const nsIID& aIID,
                                        void** aResult);
nsresult CreateNewTXTToHTMLConvFactory(nsISupports* aOuter, const nsIID& aIID,
                                       void** aResult);
nsresult CreateNewHTTPCompressConvFactory(nsISupports* aOuter,
                                          const nsIID& aIID, void** aResult);
nsresult CreateNewUnknownDecoderFactory(nsISupports* aOuter, const nsIID& aIID,
                                        void** aResult);
nsresult CreateNewBinaryDetectorFactory(nsISupports* aOuter, const nsIID& aIID,
                                        void** aResult);
nsresult nsLoadGroupConstructor(nsISupports* aOuter, const nsIID& aIID,
                                void** aResult);

extern nsresult net_NewIncrementalDownload(nsISupports*, const nsIID&, void**);

namespace mozilla {
namespace net {
nsresult WebSocketChannelConstructor(nsISupports* aOuter, const nsIID& aIID,
                                     void** aResult);
nsresult WebSocketSSLChannelConstructor(nsISupports* aOuter, const nsIID& aIID,
                                        void** aResult);
}  // namespace net
}  // namespace mozilla

#endif
