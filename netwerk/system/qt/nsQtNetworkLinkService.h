/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSQTNETWORKLINKSERVICE_H_
#define NSQTNETWORKLINKSERVICE_H_

#include "nsINetworkLinkService.h"
#include "nsIObserver.h"

class nsQtNetworkLinkService: public nsINetworkLinkService,
                              public nsIObserver
{

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSINETWORKLINKSERVICE
  NS_DECL_NSIOBSERVER

  nsQtNetworkLinkService();

  nsresult Init();
  nsresult Shutdown();

private:
  virtual ~nsQtNetworkLinkService();
};

#endif /* NSQTNETWORKLINKSERVICE_H_ */
