/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sync

import mozilla.components.concept.storage.SyncStatus
import mozilla.components.service.fxa.FirefoxAccount
import mozilla.components.service.fxa.FirefoxAccountShaped
import java.lang.Exception

/**
 * An auth-related exception type, for use with [AuthException].
 */
enum class AuthExceptionType(val msg: String) {
    KEY_INFO("Missing key info")
}

/**
 * An exception which may happen while obtaining auth information using [FirefoxAccount].
 */
class AuthException(type: AuthExceptionType) : Exception(type.msg)

/**
 * A set of results of running a sync operation for all configured stores.
 */
typealias SyncResult = Map<String, StoreSyncStatus>
data class StoreSyncStatus(val status: SyncStatus)

/**
 * A Firefox Sync friendly auth object which can be obtained from [FirefoxAccount].
 */
data class FxaAuthInfo(
    val kid: String,
    val fxaAccessToken: String,
    val syncKey: String,
    val tokenServerUrl: String
)

@Suppress("ThrowsCount")
internal suspend fun FirefoxAccountShaped.authInfo(): FxaAuthInfo {
    val syncScope = "https://identity.mozilla.com/apps/oldsync"
    val tokenServerURL = this.getTokenServerEndpointURL()
    val tokenInfo = this.getAccessToken(syncScope).await()
    val keyInfo = tokenInfo.key ?: throw AuthException(AuthExceptionType.KEY_INFO)

    return FxaAuthInfo(
        kid = keyInfo.kid,
        fxaAccessToken = tokenInfo.token,
        syncKey = keyInfo.k,
        tokenServerUrl = tokenServerURL
    )
}
