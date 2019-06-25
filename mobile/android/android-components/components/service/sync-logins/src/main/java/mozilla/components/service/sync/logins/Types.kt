/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.logins

import mozilla.components.concept.sync.SyncAuthInfo

/**
 * Conversion from a generic AuthInfo type into a type 'logins' lib uses at the interface boundary.
 */
fun SyncAuthInfo.into(): SyncUnlockInfo {
    return SyncUnlockInfo(
        kid = this.kid,
        fxaAccessToken = this.fxaAccessToken,
        syncKey = this.syncKey,
        tokenserverURL = this.tokenServerUrl
    )
}
