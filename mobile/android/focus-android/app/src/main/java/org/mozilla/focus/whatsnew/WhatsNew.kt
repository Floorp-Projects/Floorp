/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.whatsnew

import android.content.Context
import androidx.annotation.VisibleForTesting

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
class WhatsNew private constructor(private val storage: WhatsNewStorage) {

    private fun hasBeenUpdatedRecently(currentVersion: WhatsNewVersion): Boolean = when {
        hasAppBeenUpdatedRecently(currentVersion) -> {
            // The app has just been updated. Remember the new app version name and
            // reset the session counter so that we will still consider the app to be
            // updated for the next app starts.
            storage.setVersion(currentVersion)
            storage.setSessionCounter(SESSIONS_PER_UPDATE)
            true
        }
        else -> {
            // If the app has been updated recently then decrement our session
            // counter. If the counter reaches 0 we will not consider the app to
            // be updated anymore (until the app version name changes again)
            decrementAndGetSessionCounter() > 0
        }
    }

    private fun decrementAndGetSessionCounter(): Int {
        val cachedSessionCounter = storage.getSessionCounter()

        val newSessionCounter = maxOf(cachedSessionCounter - 1, 0)
        storage.setSessionCounter(newSessionCounter)

        return newSessionCounter
    }

    private fun hasAppBeenUpdatedRecently(currentVersion: WhatsNewVersion): Boolean {
        val lastKnownAppVersion = storage.getVersion()

        return lastKnownAppVersion?.let {
            currentVersion.majorVersionNumber > it.majorVersionNumber
        } ?: run { storage.setVersion(currentVersion); false }
    }

    private fun clearWhatsNewCounter() {
        storage.setSessionCounter(0)
    }

    companion object {
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
        fun shouldHighlightWhatsNew(currentVersion: WhatsNewVersion, storage: WhatsNewStorage): Boolean {
            // Cache the value for the lifetime of this process (or until userViewedWhatsNew() is called)
            if (wasUpdatedRecently == null) {
                val whatsNew = WhatsNew(storage)
                wasUpdatedRecently = whatsNew.hasBeenUpdatedRecently(currentVersion)
            }

            return wasUpdatedRecently!!
        }

        /**
         * Convenience function to run from the context.
         */
        @JvmStatic
        fun shouldHighlightWhatsNew(context: Context): Boolean {
            return shouldHighlightWhatsNew(
                ContextWhatsNewVersion(context),
                SharedPreferenceWhatsNewStorage(context)
            )
        }

        /**
         * Reset the "updated" state and continue as if the app was not updated recently.
         */
        @JvmStatic
        private fun userViewedWhatsNew(storage: WhatsNewStorage) {
            wasUpdatedRecently = false
            WhatsNew(storage).clearWhatsNewCounter()
        }

        /**
         * Convenience function to run from the context.
         */
        @JvmStatic
        fun userViewedWhatsNew(context: Context) {
            userViewedWhatsNew(SharedPreferenceWhatsNewStorage(context))
        }
    }
}
