/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpNTLMAuth_h__
#define nsHttpNTLMAuth_h__

#include "nsIHttpAuthenticator.h"

namespace mozilla { namespace net {

class nsHttpNTLMAuth : public nsIHttpAuthenticator
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIHTTPAUTHENTICATOR

    nsHttpNTLMAuth() {}

private:
    virtual ~nsHttpNTLMAuth() {}

    // This flag indicates whether we are using the native NTLM implementation
    // or the internal one.
    bool  mUseNative;
};

}} // namespace mozilla::net

#endif // !nsHttpNTLMAuth_h__
