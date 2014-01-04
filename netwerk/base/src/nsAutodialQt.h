/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSAUTODIALQT
#define NSAUTODIALQT

#include "nspr.h"
#include "nscore.h"

class nsAutodial
{
public:
    nsAutodial();
    ~nsAutodial();

    nsresult Init();

    // Dial the default RAS dialup connection.
    nsresult DialDefault(const char16_t* hostName);

    // Should we try to dial on network error?
    bool ShouldDialOnNetworkError();
};

#endif /* NSAUTODIALQT */
