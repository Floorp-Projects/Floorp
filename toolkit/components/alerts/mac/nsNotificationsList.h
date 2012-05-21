/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNotificationsList_h_
#define nsNotificationsList_h_

#include "nsINotificationsList.h"
#import "mozGrowlDelegate.h"

class nsNotificationsList : public nsINotificationsList
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSINOTIFICATIONSLIST

  nsNotificationsList();

  void informController(mozGrowlDelegate *aCont);
private:
  virtual ~nsNotificationsList();

  NSMutableArray *mNames;
  NSMutableArray *mEnabled;
};

#endif // nsNotificationsList_h_
