/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.whatsnew

import androidx.preference.PreferenceManager
import androidx.test.core.app.ApplicationProvider
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class WhatsNewTest {
    @Before
    fun setUp() {
        // Reset all saved and cached values before running a test

        PreferenceManager.getDefaultSharedPreferences(ApplicationProvider.getApplicationContext())
            .edit()
            .clear()
            .apply()

        WhatsNew.wasUpdatedRecently = null
    }

    /**
     * The first installation should not be tracked as an app update.
     */
    @Test
    fun testDefault() {
        assertFalse(WhatsNew.shouldHighlightWhatsNew(ApplicationProvider.getApplicationContext()))
    }

    @Test
    fun testWithUpdatedAppVersionName() {
        assertFalse(WhatsNew.shouldHighlightWhatsNew(ApplicationProvider.getApplicationContext()))

        val storage = object : WhatsNewStorage {
            private var version = WhatsNewVersion("2.0.0")
            private var sessionCounter = 0

            override fun getVersion(): WhatsNewVersion? {
                return version
            }

            override fun getSessionCounter(): Int {
                return sessionCounter
            }

            override fun setSessionCounter(sessionCount: Int) {
                sessionCounter = sessionCount
            }

            override fun setVersion(version: WhatsNewVersion) {
                this.version = version
            }
        }

        // Reset cached value
        WhatsNew.wasUpdatedRecently = null
        val newVersion = WhatsNewVersion("3.0.0")

        // The app was updated recently
        assertTrue(WhatsNew.shouldHighlightWhatsNew(newVersion, storage))

        // shouldHighlightWhatsNew() will continue to return true as long as the process is alive
        for (i in 1..10) {
            assertTrue(WhatsNew.shouldHighlightWhatsNew(newVersion, storage))
        }
    }

    @Test
    fun testOverMultipleSessions() {
        assertFalse(WhatsNew.shouldHighlightWhatsNew(ApplicationProvider.getApplicationContext()))

        PreferenceManager.getDefaultSharedPreferences(ApplicationProvider.getApplicationContext())
            .edit()
            .putString(WhatsNewStorage.PREFERENCE_KEY_APP_NAME, "2.0")
            .apply()

        for (i in 1..3) {
            // Reset cached value
            WhatsNew.wasUpdatedRecently = null
            assertTrue(WhatsNew.shouldHighlightWhatsNew(ApplicationProvider.getApplicationContext()))
        }

        // After three sessions the method will return false again
        WhatsNew.wasUpdatedRecently = null
        assertFalse(WhatsNew.shouldHighlightWhatsNew(ApplicationProvider.getApplicationContext()))
    }

    @Test
    fun testResettingManually() {
        assertFalse(WhatsNew.shouldHighlightWhatsNew(ApplicationProvider.getApplicationContext()))

        PreferenceManager.getDefaultSharedPreferences(ApplicationProvider.getApplicationContext())
            .edit()
            .putString(WhatsNewStorage.PREFERENCE_KEY_APP_NAME, "2.0")
            .apply()

        // Reset cached value
        WhatsNew.wasUpdatedRecently = null

        assertTrue(WhatsNew.shouldHighlightWhatsNew(ApplicationProvider.getApplicationContext()))

        // Now we reset the state manually
        WhatsNew.userViewedWhatsNew(ApplicationProvider.getApplicationContext())

        assertFalse(WhatsNew.shouldHighlightWhatsNew(ApplicationProvider.getApplicationContext()))

        // And also the next time we will return false
        WhatsNew.wasUpdatedRecently = null
        assertFalse(WhatsNew.shouldHighlightWhatsNew(ApplicationProvider.getApplicationContext()))
    }
}
