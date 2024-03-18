/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.whatsnew

import android.content.Context
import android.content.SharedPreferences
import androidx.preference.PreferenceManager
import java.util.concurrent.TimeUnit

// This file is a modified port from Focus Android

/**
 * Interface to abstract where the cached version and session counter is stored
 */
interface WhatsNewStorage {
    fun getVersion(): WhatsNewVersion?
    fun setVersion(version: WhatsNewVersion)
    fun getWhatsNewHasBeenCleared(): Boolean
    fun setWhatsNewHasBeenCleared(cleared: Boolean)
    fun getDaysSinceUpdate(): Long
    fun setDateOfUpdate(day: Long)

    companion object {
        internal const val PREFERENCE_KEY_APP_NAME = "whatsnew-lastKnownAppVersionName"
        internal const val PREFERENCE_KEY_WHATS_NEW_CLEARED = "whatsnew-cleared"
        internal const val PREFERENCE_KEY_UPDATE_DAY = "whatsnew-lastKnownAppVersionUpdateDay"
    }
}

class SharedPreferenceWhatsNewStorage(private val sharedPreference: SharedPreferences) :
    WhatsNewStorage {

    constructor(context: Context) : this(PreferenceManager.getDefaultSharedPreferences(context))

    override fun getVersion(): WhatsNewVersion? {
        return sharedPreference.getString(WhatsNewStorage.PREFERENCE_KEY_APP_NAME, null)?.let {
            WhatsNewVersion(it)
        }
    }

    override fun setVersion(version: WhatsNewVersion) {
        sharedPreference.edit()
            .putString(WhatsNewStorage.PREFERENCE_KEY_APP_NAME, version.version)
            .apply()
    }

    override fun getWhatsNewHasBeenCleared(): Boolean {
        return sharedPreference.getBoolean(WhatsNewStorage.PREFERENCE_KEY_WHATS_NEW_CLEARED, false)
    }

    override fun setWhatsNewHasBeenCleared(cleared: Boolean) {
        sharedPreference.edit()
            .putBoolean(WhatsNewStorage.PREFERENCE_KEY_WHATS_NEW_CLEARED, cleared)
            .apply()
    }

    override fun getDaysSinceUpdate(): Long {
        val updateDay = sharedPreference.getLong(WhatsNewStorage.PREFERENCE_KEY_UPDATE_DAY, 0)
        return TimeUnit.MILLISECONDS.toDays(System.currentTimeMillis() - updateDay)
    }

    override fun setDateOfUpdate(day: Long) {
        sharedPreference.edit()
            .putLong(WhatsNewStorage.PREFERENCE_KEY_UPDATE_DAY, day)
            .apply()
    }
}
