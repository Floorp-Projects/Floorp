/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http:mozilla.org/MPL/2.0/. */

#include "mozilla/net/DashboardTypes.h"

#ifndef nsHttpInfo__
#define nsHttpInfo__

namespace mozilla {
namespace net {

class HttpInfo
{
public:
    /* Calls getConnectionData method in nsHttpConnectionMgr. */
    static void GetHttpConnectionData(nsTArray<HttpRetParams> *);
};

} } // namespace mozilla::net

#endif // nsHttpInfo__
