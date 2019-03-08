/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

import android.content.Context
import java.util.UUID

/**
 * Class used to generate a random UUID for the device
 * and store it persistent in shared preferences.
 *
 * If the UUID was already generated it returns the stored one
 *
 * @param context context
 */
internal class DeviceUuidFactory(context: Context) {
    /**
     * Unique UUID for the current android device. As with all UUIDs,
     * this unique ID is "very highly likely" to be unique across all Android
     * devices. Much more so than ANDROID_ID is
     *
     * The UUID is generated with <code>UUID.randomUUID()</code>
     */
    val uuid by lazy {
        val preferences = context
            .getSharedPreferences(PREFS_FILE, Context.MODE_PRIVATE)
        val prefUuid = preferences.getString(PREF_UUID_KEY, null)

        if (prefUuid != null) {
            UUID.fromString(prefUuid).toString()
        } else {
            val uuid = UUID.randomUUID()
            preferences.edit().putString(PREF_UUID_KEY, uuid.toString()).apply()
            uuid.toString()
        }
    }

    companion object {
        private const val PREFS_FILE = "mozilla.components.service.experiments"
        private const val PREF_UUID_KEY = "device_uuid"
    }
}
