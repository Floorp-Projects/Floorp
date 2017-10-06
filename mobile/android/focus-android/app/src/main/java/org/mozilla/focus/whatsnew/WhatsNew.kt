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
 *
 * The application is considered updated when the application's version name changes (versionName
 * in the manifest). The applications version code would be a good candidates too, but it might
 * change more often (RC builds) without the application actually changing from the user's point
 * of view.
 *
 * Whenever the application was updated we still consider the application to be "recently updated"
 * for the next couple of (process) starts.
 */
class WhatsNew {
    companion object {
        // TODO: Update to final URL
        const val SUMO_URL = "https://support.mozilla.org/en-US/products/focus-firefox/firefox-focus-android"

        @VisibleForTesting const val PREFERENCE_KEY_APP_NAME = "whatsnew-lastKnownAppVersionName"
        private const val PREFERENCE_KEY_SESSION_COUNTER = "whatsnew-updateSessionCounter"

        /**
         * How many "sessions" do we consider the app to be updated? In this case a session means
         * process creations.
         */
        private const val SESSIONS_PER_UPDATE = 3

        @VisibleForTesting var wasUpdatedRecently: Boolean? = null

        /**
         * Has this app been updated recently. Either this is the first start of the application
         * since it was updated or this is a later start but still recent enough to consider the
         * app to be updated recently.
         */
        fun wasUpdatedRecently(context: Context): Boolean {
            var recentlyUpdated = wasUpdatedRecently

            if (recentlyUpdated == null) {
                recentlyUpdated = when {
                    hasAppBeenUpdated(context) -> {
                        // The app has just been updated. Remember the new app version name and
                        // reset the session counter so that we will still consider the app to be
                        // updated for the next app starts.
                        saveAppVersionName(context)
                        setSessionCounter(context, SESSIONS_PER_UPDATE)
                        true
                    }
                    else -> {
                        // If the app has been updated recently then decrement our session
                        // counter. If the counter reaches 0 we will not consider the app to
                        // be updated anymore (until the app version name changes again)
                        decrementSessionCounter(context)
                        hasBeenUpdatedInRecentSession(context)
                    }
                }
                // Cache the value for the lifetime of this process (or until reset() is called)
                wasUpdatedRecently = recentlyUpdated
            }
            return recentlyUpdated
        }

        /**
         * Reset the "updated" state and continue as if the app was not updated recently.
         */
        @JvmStatic
        fun reset(context: Context) {
            wasUpdatedRecently = false
            setSessionCounter(context, 0)
        }

        private fun hasAppBeenUpdated(context: Context): Boolean {
            val lastKnownAppVersionName = getLastKnownAppVersionName(context)
            if (lastKnownAppVersionName.isNullOrEmpty()) {
                saveAppVersionName(context)
                return false
            }

            return !lastKnownAppVersionName.equals(context.appVersionName)
        }

        private fun getLastKnownAppVersionName(context: Context): String? {
            return PreferenceManager.getDefaultSharedPreferences(context)
                    .getString(PREFERENCE_KEY_APP_NAME, null)
        }

        private fun saveAppVersionName(context: Context) {
            PreferenceManager.getDefaultSharedPreferences(context)
                    .edit()
                    .putString(PREFERENCE_KEY_APP_NAME, context.appVersionName)
                    .apply()
        }

        private fun setSessionCounter(context: Context, count: Int) {
            PreferenceManager.getDefaultSharedPreferences(context)
                    .edit()
                    .putInt(PREFERENCE_KEY_SESSION_COUNTER, count)
                    .apply()
        }

        private fun decrementSessionCounter(context: Context) {
            val sessionCounter = PreferenceManager.getDefaultSharedPreferences(context)
                    .getInt(PREFERENCE_KEY_SESSION_COUNTER, 0)

            setSessionCounter(context, maxOf(sessionCounter - 1, 0))
        }

        private fun hasBeenUpdatedInRecentSession(context: Context): Boolean {
            val sessionCounter = PreferenceManager.getDefaultSharedPreferences(context)
                    .getInt(PREFERENCE_KEY_SESSION_COUNTER, 0)

            return sessionCounter > 0
        }
    }
}
