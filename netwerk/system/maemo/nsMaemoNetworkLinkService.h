/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is MaemoAutodial.
 *
 * The Initial Developer of the Original Code is
 * Nokia Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Jeremias Bosch <jeremias.bosch@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef NSMAEMONETWORKLINKSERVICE_H_
#define NSMAEMONETWORKLINKSERVICE_H_

#include "nsINetworkLinkService.h"
#include "nsIObserver.h"

class nsMaemoNetworkLinkService: public nsINetworkLinkService,
                                 public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSINETWORKLINKSERVICE
  NS_DECL_NSIOBSERVER

  nsMaemoNetworkLinkService();
  virtual ~nsMaemoNetworkLinkService();

  nsresult Init();
  nsresult Shutdown();
};

#endif /* NSMAEMONETWORKLINKSERVICE_H_ */
