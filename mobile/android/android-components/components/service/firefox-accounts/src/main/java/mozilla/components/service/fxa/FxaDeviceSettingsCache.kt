/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.service.fxa

import android.content.Context
import android.content.SharedPreferences
import mozilla.appservices.syncmanager.DeviceSettings
import mozilla.appservices.syncmanager.DeviceType
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.utils.SharedPreferencesCache
import org.json.JSONObject
import java.lang.IllegalArgumentException
import java.lang.IllegalStateException

private const val CACHE_NAME = "FxaDeviceSettingsCache"
private const val CACHE_KEY = CACHE_NAME
private const val KEY_FXA_DEVICE_ID = "kid"
private const val KEY_DEVICE_NAME = "syncKey"
private const val KEY_DEVICE_TYPE = "tokenServerUrl"

/**
 * A thin wrapper around [SharedPreferences] which knows how to serialize/deserialize [DeviceSettings].
 *
 * This class exists to provide background sync workers with access to [DeviceSettings].
 */
class FxaDeviceSettingsCache(context: Context) : SharedPreferencesCache<DeviceSettings>(context) {
    override val logger = Logger("SyncAuthInfoCache")
    override val cacheKey = CACHE_KEY
    override val cacheName = CACHE_NAME

    override fun DeviceSettings.toJSON(): JSONObject {
        return JSONObject().also {
            it.put(KEY_FXA_DEVICE_ID, this.fxaDeviceId)
            it.put(KEY_DEVICE_NAME, this.name)
            it.put(KEY_DEVICE_TYPE, this.kind.toString())
        }
    }

    override fun fromJSON(obj: JSONObject): DeviceSettings {
        return DeviceSettings(
            fxaDeviceId = obj.getString(KEY_FXA_DEVICE_ID),
            name = obj.getString(KEY_DEVICE_NAME),
            kind = obj.getString(KEY_DEVICE_TYPE).toDeviceType(),
        )
    }

    /**
     * @param name New device name to write into the cache.
     */
    fun updateCachedName(name: String) {
        val cached = getCached() ?: throw IllegalStateException("Trying to update cached value in an empty cache")
        setToCache(cached.copy(name = name))
    }

    private fun String.toDeviceType(): DeviceType {
        return when (this) {
            "DESKTOP" -> DeviceType.DESKTOP
            "MOBILE" -> DeviceType.MOBILE
            "TABLET" -> DeviceType.TABLET
            "VR" -> DeviceType.VR
            "TV" -> DeviceType.TV
            else -> throw IllegalArgumentException("Unknown device type in cached string: $this")
        }
    }
}
