/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.whatsnew

import android.preference.PreferenceManager
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class WhatsNewStorageTest {
    @Before
    fun setUp() {
        // Reset all saved and cached values before running a test
        PreferenceManager.getDefaultSharedPreferences(RuntimeEnvironment.application)
            .edit()
            .clear()
            .apply()
    }

    @Test
    fun testGettingAndSettingAVersion() {
        val storage = SharedPreferenceWhatsNewStorage(RuntimeEnvironment.application)

        val version = WhatsNewVersion("3.0")
        storage.setVersion(version)

        val storedVersion = storage.getVersion()
        assertEquals(version, storedVersion)
    }

    @Test
    fun testGettingAndSettingTheSessionCounter() {
        val storage = SharedPreferenceWhatsNewStorage(RuntimeEnvironment.application)
        val expected = 3

        storage.setSessionCounter(expected)

        val storedCounter = storage.getSessionCounter()
        assertEquals(expected, storedCounter)
    }
}
