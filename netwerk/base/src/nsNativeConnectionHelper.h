/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNativeConnectionHelper_h__
#define nsNativeConnectionHelper_h__

#include "nscore.h"

class nsISocketTransport;

class nsNativeConnectionHelper
{
public:
    /**
     * OnConnectionFailed
     *
     * Return true if the connection should be re-attempted.
     */
    static bool OnConnectionFailed(const PRUnichar* hostName);

    /**
     * IsAutoDialEnabled
     *
     * Return true if autodial is enabled in the operating system.
     */   
    static bool IsAutodialEnabled();
};

#endif // !nsNativeConnectionHelper_h__
