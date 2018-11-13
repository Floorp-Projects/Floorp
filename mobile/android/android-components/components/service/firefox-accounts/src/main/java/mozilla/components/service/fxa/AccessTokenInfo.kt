/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import org.json.JSONObject
import org.mozilla.fxaclient.internal.AccessTokenInfo as InternalAccessTokenInfo

/**
 * Scoped key data.
 *
 * @property kid The JWK key identifier.
 * @property k The JWK key data.
 */
data class OAuthScopedKey(
    val kid: String,
    val k: String
)

/**
 * The result of authentication with FxA via an OAuth flow.
 *
 * @property token The access token produced by the flow.
 * @property key An OAuthScopedKey if present.
 * @property expiresAt The expiry date timestamp of this token since unix epoch (in seconds).
 */
data class AccessTokenInfo(
    val token: String,
    val key: OAuthScopedKey?,
    val expiresAt: Long
) {
    companion object {
        internal fun fromInternal(v: InternalAccessTokenInfo): AccessTokenInfo {

            // v.key (when present) is a stringified JSON object that represents a key.
            // We don't include the "kty" which is always the string "oct".
            val key = v.key?.let {
                val obj = JSONObject(it)
                OAuthScopedKey(
                        kid = obj.getString("kid"),
                        k = obj.getString("k")
                )
            }

            return AccessTokenInfo(
                    // The access token should always be present
                    token = v.token!!,
                    key = key,
                    expiresAt = v.expiresAt
            )
        }
    }
}
