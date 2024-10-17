/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MicrosoftEntraSSOUtils_h__
#define MicrosoftEntraSSOUtils_h__

namespace mozilla {
namespace net {

class nsHttpChannel;
class MicrosoftEntraSSOUtils;  // Used in SSORequestDelegate

void AddMicrosoftEntraSSO(nsHttpChannel* aChannel);

}  // namespace net
}  // namespace mozilla

#endif  // MicrosoftEntraSSOUtils_h__
