/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _MAC_WEB_APP_UTILS_H_
#define _MAC_WEB_APP_UTILS_H_

#include "nsIMacWebAppUtils.h"

#define NS_MACWEBAPPUTILS_CONTRACTID "@mozilla.org/widget/mac-web-app-utils;1"

class nsMacWebAppUtils : public nsIMacWebAppUtils {
 public:
  nsMacWebAppUtils() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMACWEBAPPUTILS

 protected:
  virtual ~nsMacWebAppUtils() {}
};

#endif  //_MAC_WEB_APP_UTILS_H_
