/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.service.fxa

import android.content.Context
import android.content.SharedPreferences
import mozilla.components.concept.sync.SyncAuthInfo
import mozilla.components.support.base.log.logger.Logger
import org.json.JSONException
import org.json.JSONObject

private const val CACHE_NAME = "SyncAuthInfoCache"
private const val CACHE_KEY = CACHE_NAME
private const val KEY_FXA_ACCESS_TOKEN = "fxaAccessToken"
private const val KEY_FXA_ACCESS_TOKEN_EXPIRES_AT = "fxaAccessTokenExpiresAt"
private const val KEY_KID = "kid"
private const val KEY_SYNC_KEY = "syncKey"
private const val KEY_TOKEN_SERVER_URL = "tokenServerUrl"

/**
 * A thin wrapper around [SharedPreferences] which knows how to serialize/deserialize [SyncAuthInfo].
 */
class SyncAuthInfoCache(val context: Context) {
    private val logger = Logger("SyncAuthInfoCache")

    fun setToCache(authInfo: SyncAuthInfo) {
        cache().edit().putString(CACHE_KEY, authInfo.toCacheString()).apply()
    }

    fun getCached(): SyncAuthInfo? {
        val s = cache().getString(CACHE_KEY, null) ?: return null
        return fromCacheString(s)
    }

    fun expired(): Boolean {
        val s = cache().getString(CACHE_KEY, null) ?: return true
        val cached = fromCacheString(s) ?: return true
        return cached.fxaAccessTokenExpiresAt <= System.currentTimeMillis()
    }

    fun clear() {
        cache().edit().clear().apply()
    }

    private fun cache(): SharedPreferences {
        return context.getSharedPreferences(CACHE_NAME, Context.MODE_PRIVATE)
    }

    private fun SyncAuthInfo.toCacheString(): String? {
        val o = JSONObject()
        o.put(KEY_KID, this.kid)
        o.put(KEY_FXA_ACCESS_TOKEN, this.fxaAccessToken)
        o.put(KEY_FXA_ACCESS_TOKEN_EXPIRES_AT, this.fxaAccessTokenExpiresAt)
        o.put(KEY_SYNC_KEY, this.syncKey)
        o.put(KEY_TOKEN_SERVER_URL, this.tokenServerUrl)
        // JSONObject swallows any JSONExceptions thrown during 'toString', and simply returns 'null'.
        val asString: String? = o.toString()
        if (asString == null) {
            // An error happened while converting a JSONObject into a string. Let's fail loudly and
            // see if this actually happens in the wild. An alternative is to swallow this error and
            // log an error message, but we're very unlikely to notice any problems in that case.
            throw IllegalStateException("Failed to stringify SyncAuthInfo")
        }
        return asString
    }

    private fun fromCacheString(s: String): SyncAuthInfo? {
        return try {
            val o = JSONObject(s)
            SyncAuthInfo(
                kid = o.getString(KEY_KID),
                fxaAccessToken = o.getString(KEY_FXA_ACCESS_TOKEN),
                fxaAccessTokenExpiresAt = o.getLong(KEY_FXA_ACCESS_TOKEN_EXPIRES_AT),
                syncKey = o.getString(KEY_SYNC_KEY),
                tokenServerUrl = o.getString(KEY_TOKEN_SERVER_URL)
            )
        } catch (e: JSONException) {
            logger.error("Failed to parse cached value", e)
            null
        }
    }
}
