/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is Growl implementation of nsIAlertsService.
 *
 * The Initial Developer of the Original Code is
 *   Shawn Wilsher <me@shawnwilsher.com>.
 * Portions created by the Initial Developer are Copyright (C) 2006-2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsAlertsService_h_
#define nsAlertsService_h_

#include "nsIAlertsService.h"
#include "nsIObserver.h"
#include "nsToolkitCompsCID.h"

struct GrowlDelegateWrapper;

class nsAlertsService : public nsIAlertsService,
                        public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIALERTSSERVICE
  NS_DECL_NSIOBSERVER

  nsAlertsService();
  nsresult Init();
private:
  GrowlDelegateWrapper* mDelegate;
  virtual ~nsAlertsService();
};

class nsModuleComponentInfo;
NS_METHOD nsAlertsServiceRegister(nsIComponentManager* aCompMgr,
                                  nsIFile *aPath,
                                  const char* registryLocation,
                                  const char* componentType,
                                  const nsModuleComponentInfo* info);
NS_METHOD nsAlertsServiceUnregister(nsIComponentManager* aCompMgr,
                                    nsIFile* aPath,
                                    const char* registryLocation,
                                    const nsModuleComponentInfo* info);

#endif // nsAlertsService_h_
