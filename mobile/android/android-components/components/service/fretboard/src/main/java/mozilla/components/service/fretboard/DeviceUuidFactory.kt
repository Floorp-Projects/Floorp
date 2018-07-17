/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard

import android.content.Context
import java.util.UUID

internal class DeviceUuidFactory(context: Context) {
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
        private const val PREFS_FILE = "mozilla.components.service.fretboard"
        private const val PREF_UUID_KEY = "device_uuid"
    }
}
