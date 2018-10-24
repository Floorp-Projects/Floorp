/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import org.json.JSONObject
import org.mozilla.fxaclient.internal.OAuthInfo as InternalOAuthInfo

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
 * @property accessToken The access token produced by the flow.
 * @property keys When present, a map of scopes to [OAuthScopedKey].
 * @property scopes A list of scopes that this token is valid for.
 */
data class OAuthInfo(
    val accessToken: String,
    val keys: Map<String, OAuthScopedKey>?,
    val scopes: List<String>
) {
    companion object {
        internal fun fromInternal(v: InternalOAuthInfo): OAuthInfo {
            // This is a space-separated list of scopes.
            val scopes = v.scope?.split(" ")?.filter { it.isNotEmpty() } ?: listOf()

            // v.keys (when present) is a string representing a JSON object that maps scopes to
            // keys. There's some extra data in each key that we don't include ("kty" which is
            // always the string "oct", and "scope" which is always the key in the map).
            val keys = v.keys?.let {
                val jo = JSONObject(it)
                val result = mutableMapOf<String, OAuthScopedKey>()
                for (scope in jo.keys()) {
                    val obj = jo.getJSONObject(scope)
                    result[scope] = OAuthScopedKey(
                            kid = obj.getString("kid"),
                            k = obj.getString("k")
                    )
                }
                result
            }

            return OAuthInfo(
                    // The access token should always be present
                    accessToken = v.accessToken!!,
                    keys = keys,
                    scopes = scopes
            )
        }
    }
}
