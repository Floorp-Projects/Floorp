/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.whatsnew

import android.content.Context
import android.os.StrictMode
import org.mozilla.fenix.ext.components

// This file is a modified port from Focus Android

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
 * for the next few days.
 */
class WhatsNew private constructor(private val storage: WhatsNewStorage) {

    private fun hasBeenUpdatedRecently(currentVersion: WhatsNewVersion): Boolean {
        val lastKnownAppVersion = storage.getVersion()

        // Update the version and date if *just* updated
        if (lastKnownAppVersion == null ||
            currentVersion.majorVersionNumber > lastKnownAppVersion.majorVersionNumber
        ) {
            storage.setVersion(currentVersion)
            storage.setDateOfUpdate(System.currentTimeMillis())
            return true
        }

        return (!storage.getWhatsNewHasBeenCleared() && storage.getDaysSinceUpdate() < DAYS_PER_UPDATE)
    }

    companion object {
        /**
         * How many days do we consider the app to be updated?
         */
        private const val DAYS_PER_UPDATE = 3

        internal var wasUpdatedRecently: Boolean? = null

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
        fun shouldHighlightWhatsNew(context: Context): Boolean {
            return shouldHighlightWhatsNew(
                ContextWhatsNewVersion(context),
                context.components.strictMode.resetAfter(StrictMode.allowThreadDiskReads()) {
                    SharedPreferenceWhatsNewStorage(context)
                },
            )
        }

        /**
         * Reset the "updated" state and continue as if the app was not updated recently.
         */
        @JvmStatic
        private fun userViewedWhatsNew(storage: WhatsNewStorage) {
            wasUpdatedRecently = false
            storage.setWhatsNewHasBeenCleared(true)
        }

        /**
         * Convenience function to run from the context.
         */
        @JvmStatic
        fun userViewedWhatsNew(context: Context) {
            userViewedWhatsNew(
                SharedPreferenceWhatsNewStorage(
                    context,
                ),
            )
        }
    }
}
