/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*****************************************************************************/
/* This is an automatically generated file. If you're not                    */
/* nsSiteSecurityService.cpp, you shouldn't be #including it.     */
/*****************************************************************************/

#include <stdint.h>
const PRTime gPreloadListExpirationTime = INT64_C(1387016108776000);

class nsSTSPreload
{
  public:
    const char *mHost;
    const bool mIncludeSubdomains;
};

static const nsSTSPreload kSTSPreloadList[] = {
  { "alpha.irccloud.com", false },
  { "haste.ch", true },
  { "passwd.io", true },
  { "piratenlogin.de", true },
};
