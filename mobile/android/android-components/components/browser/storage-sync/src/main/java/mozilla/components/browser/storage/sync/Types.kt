/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.sync

import mozilla.components.concept.storage.VisitType
import mozilla.components.concept.sync.AuthInfo

// We have type definitions at the concept level, and "external" types defined within Places.
// In practice these two types are largely the same, and this file is the conversion point.

/**
 * Conversion from a generic AuthInfo type into a type 'places' lib uses at the interface boundary.
 */
internal fun AuthInfo.into(): SyncAuthInfo {
    return SyncAuthInfo(
        kid = this.kid,
        fxaAccessToken = this.fxaAccessToken,
        syncKey = this.syncKey,
        tokenserverURL = this.tokenServerUrl
    )
}

/**
 * Conversion from a generic [VisitType] into its richer comrade within the 'places' lib.
 */
internal fun VisitType.into(): org.mozilla.places.VisitType {
    return when (this) {
        VisitType.LINK -> org.mozilla.places.VisitType.LINK
        VisitType.RELOAD -> org.mozilla.places.VisitType.RELOAD
        VisitType.TYPED -> org.mozilla.places.VisitType.TYPED
    }
}
