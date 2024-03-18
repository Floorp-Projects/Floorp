/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.service.fxa

import android.content.Context
import android.content.SharedPreferences
import mozilla.components.concept.sync.SyncAuthInfo
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.utils.SharedPreferencesCache
import org.json.JSONObject
import java.util.concurrent.TimeUnit

private const val CACHE_NAME = "SyncAuthInfoCache"
private const val CACHE_KEY = CACHE_NAME
private const val KEY_FXA_ACCESS_TOKEN = "fxaAccessToken"
private const val KEY_FXA_ACCESS_TOKEN_EXPIRES_AT = "fxaAccessTokenExpiresAt"
private const val KEY_KID = "kid"
private const val KEY_SYNC_KEY = "syncKey"
private const val KEY_TOKEN_SERVER_URL = "tokenServerUrl"

/**
 * A thin wrapper around [SharedPreferences] which knows how to serialize/deserialize [SyncAuthInfo].
 *
 * This class exists to provide background sync workers with access to [SyncAuthInfo].
 */
class SyncAuthInfoCache(context: Context) : SharedPreferencesCache<SyncAuthInfo>(context) {
    override val logger = Logger("SyncAuthInfoCache")
    override val cacheKey = CACHE_KEY
    override val cacheName = CACHE_NAME

    override fun SyncAuthInfo.toJSON(): JSONObject {
        return JSONObject().also {
            it.put(KEY_KID, this.kid)
            it.put(KEY_FXA_ACCESS_TOKEN, this.fxaAccessToken)
            it.put(KEY_FXA_ACCESS_TOKEN_EXPIRES_AT, this.fxaAccessTokenExpiresAt)
            it.put(KEY_SYNC_KEY, this.syncKey)
            it.put(KEY_TOKEN_SERVER_URL, this.tokenServerUrl)
        }
    }

    override fun fromJSON(obj: JSONObject): SyncAuthInfo {
        return SyncAuthInfo(
            kid = obj.getString(KEY_KID),
            fxaAccessToken = obj.getString(KEY_FXA_ACCESS_TOKEN),
            fxaAccessTokenExpiresAt = obj.getLong(KEY_FXA_ACCESS_TOKEN_EXPIRES_AT),
            syncKey = obj.getString(KEY_SYNC_KEY),
            tokenServerUrl = obj.getString(KEY_TOKEN_SERVER_URL),
        )
    }

    fun expired(): Boolean {
        val expiresAt = getCached()?.fxaAccessTokenExpiresAt ?: return true
        val now = TimeUnit.MILLISECONDS.toSeconds(System.currentTimeMillis())

        return expiresAt <= now
    }
}
