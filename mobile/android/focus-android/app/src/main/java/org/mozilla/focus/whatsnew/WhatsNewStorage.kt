/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.whatsnew

import android.content.Context
import android.content.SharedPreferences
import androidx.annotation.VisibleForTesting
import androidx.preference.PreferenceManager

/**
 * Interface to abstract where the cached version and session counter is stored
 */
interface WhatsNewStorage {
    fun getVersion(): WhatsNewVersion?
    fun setVersion(version: WhatsNewVersion)
    fun getSessionCounter(): Int
    fun setSessionCounter(sessionCount: Int)

    companion object {
        @VisibleForTesting
        internal const val PREFERENCE_KEY_APP_NAME = "whatsnew-lastKnownAppVersionName"

        @VisibleForTesting
        internal const val PREFERENCE_KEY_SESSION_COUNTER = "whatsnew-updateSessionCounter"
    }
}

class SharedPreferenceWhatsNewStorage(private val sharedPreference: SharedPreferences) : WhatsNewStorage {

    constructor(context: Context) : this(PreferenceManager.getDefaultSharedPreferences(context))

    override fun getVersion(): WhatsNewVersion? {
        return sharedPreference.getString(WhatsNewStorage.PREFERENCE_KEY_APP_NAME, null)?.let { WhatsNewVersion(it) }
    }

    override fun setVersion(version: WhatsNewVersion) {
        sharedPreference.edit()
            .putString(WhatsNewStorage.PREFERENCE_KEY_APP_NAME, version.version)
            .apply()
    }

    override fun getSessionCounter(): Int {
        return sharedPreference.getInt(WhatsNewStorage.PREFERENCE_KEY_SESSION_COUNTER, 0)
    }

    override fun setSessionCounter(sessionCount: Int) {
        sharedPreference.edit()
            .putInt(WhatsNewStorage.PREFERENCE_KEY_SESSION_COUNTER, sessionCount)
            .apply()
    }
}
