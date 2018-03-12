/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.whatsnew

import android.content.Context
import android.preference.PreferenceManager
import android.support.annotation.VisibleForTesting
import mozilla.components.ktx.android.content.appVersionName

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
        @VisibleForTesting const val PREFERENCE_KEY_APP_NAME = "whatsnew-lastKnownAppVersionName"
        private const val PREFERENCE_KEY_SESSION_COUNTER = "whatsnew-updateSessionCounter"

        /**
         * How many "sessions" do we consider the app to be updated? In this case a session means
         * process creations.
         */
        private const val SESSIONS_PER_UPDATE = 3

        @VisibleForTesting internal var wasUpdatedRecently: Boolean? = null

        /**
         * Should we highlight the "What's new" menu item because this app been updated recently?
         *
         * This method returns true either if this is the first start of the application since it
         * was updated or this is a later start but still recent enough to consider the app to be
         * updated recently.
         */
        @JvmStatic
        fun shouldHighlightWhatsNew(context: Context): Boolean {
            // Cache the value for the lifetime of this process (or until userViewedWhatsNew() is called)
            if (wasUpdatedRecently == null) {
                wasUpdatedRecently = wasUpdatedRecentlyInner(context)
            }
            return wasUpdatedRecently!!
        }

        private fun wasUpdatedRecentlyInner(context: Context): Boolean = when {
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
                decrementAndGetSessionCounter(context) > 0
            }
        }

        /**
         * Reset the "updated" state and continue as if the app was not updated recently.
         */
        @JvmStatic
        fun userViewedWhatsNew(context: Context) {
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

        private fun decrementAndGetSessionCounter(context: Context): Int {
            val cachedSessionCounter = PreferenceManager.getDefaultSharedPreferences(context)
                    .getInt(PREFERENCE_KEY_SESSION_COUNTER, 0)

            val newSessionCounter = maxOf(cachedSessionCounter - 1, 0)
            setSessionCounter(context, newSessionCounter)
            return newSessionCounter
        }
    }
}
