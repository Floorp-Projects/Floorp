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

#ifndef NSMAEMONETWORKMANAGER_H_
#define NSMAEMONETWORKMANAGER_H_

#include "nscore.h"

class nsMaemoNetworkManager
{
public:
  // Can be called from any thread, most likely the socket transport thread
  static PRBool OpenConnectionSync();
  static void CloseConnection();

  static PRBool IsConnected();
  static PRBool GetLinkStatusKnown();

  // Called from the nsMaemoNetworkLinkService (main thread only)
  static PRBool Startup();
  static void Shutdown();
};

#endif /* NSMAEMONETWORKMANAGER_H_ */
