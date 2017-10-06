/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.whatsnew

import android.content.Context
import android.preference.PreferenceManager
import android.support.annotation.VisibleForTesting
import org.mozilla.focus.ext.appVersionName

/**
 * Helper class tracking whether the application was recently updated in order to show "What's new"
 * menu items and indicators in the application UI.
 */
class WhatsNew {
    companion object {
        // TODO: Update to final URL
        const val SUMO_URL = "https://support.mozilla.org/en-US/products/focus-firefox/firefox-focus-android"

        @VisibleForTesting const val PREFERENCE_KEY_APP_NAME = "lastKnownAppVersionName"
        private const val PREFERENCE_KEY_SESSION_COUNTER = "updateSessionCounter"
        private const val SESSIONS_PER_UPDATE = 3

        @VisibleForTesting var wasUpdatedRecently: Boolean? = null

        /**
         * Has this app been updated recently. Either this is the first start of the application
         * since it was updated or this is a later start but still recent enough to consider the
         * app to be updated recently.
         */
        fun wasUpdatedRecently(context: Context): Boolean {
            if (wasUpdatedRecently == null) {
                wasUpdatedRecently = when {
                    hasBeenUpdated(context) -> {
                        rememberAppVersionName(context)
                        setSessionCounter(context)
                        true
                    }
                    else -> hasBeenUpdatedInRecentSession(context)
                }
            }
            return wasUpdatedRecently!!
        }

        /**
         * Reset the "updated" state and continue as if the app was not updated recently.
         */
        @JvmStatic
        fun reset(context: Context) {
            wasUpdatedRecently = false
            setSessionCounter(context, 0)
        }

        private fun hasBeenUpdated(context: Context): Boolean {
            val lastKnownAppVersionName = getLastKnownAppVersionName(context)
            if (lastKnownAppVersionName.isNullOrEmpty()) {
                rememberAppVersionName(context)
                return false
            }

            return !lastKnownAppVersionName.equals(context.appVersionName)
        }

        private fun getLastKnownAppVersionName(context: Context): String? {
            return PreferenceManager.getDefaultSharedPreferences(context)
                    .getString(PREFERENCE_KEY_APP_NAME, null)
        }

        private fun rememberAppVersionName(context: Context) {
            PreferenceManager.getDefaultSharedPreferences(context)
                    .edit()
                    .putString(PREFERENCE_KEY_APP_NAME, context.appVersionName)
                    .apply()
        }

        private fun setSessionCounter(context: Context, count: Int = SESSIONS_PER_UPDATE) {
            PreferenceManager.getDefaultSharedPreferences(context)
                    .edit()
                    .putInt(PREFERENCE_KEY_SESSION_COUNTER, count)
                    .apply()
        }

        private fun hasBeenUpdatedInRecentSession(context: Context): Boolean {
            val sessionCounter = PreferenceManager.getDefaultSharedPreferences(context)
                    .getInt(PREFERENCE_KEY_SESSION_COUNTER, 0)

            if (sessionCounter > 0) {
                setSessionCounter(context, sessionCounter - 1)
                return true
            }

            return false
        }
    }
}
