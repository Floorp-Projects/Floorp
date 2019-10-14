package org.mozilla.fenix.whatsnew

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.TestApplication
import org.mozilla.fenix.ext.clearAndCommit
import org.mozilla.fenix.utils.Settings
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
@Config(application = TestApplication::class)
class WhatsNewStorageTest {
    private lateinit var storage: SharedPreferenceWhatsNewStorage
    private lateinit var settings: Settings

    @Before
    fun setUp() {
        storage = SharedPreferenceWhatsNewStorage(testContext)
        settings = Settings.getInstance(testContext)
            .apply(Settings::clear)
    }

    @Test
    fun testGettingAndSettingAVersion() {
        val version = WhatsNewVersion("3.0")
        storage.setVersion(version)

        val storedVersion = storage.getVersion()
        Assert.assertEquals(version, storedVersion)
    }

    @Test
    fun testGettingAndSettingTheDateOfUpdate() {
        val currentTime = System.currentTimeMillis()
        val twoDaysAgo = (currentTime - DAY_IN_MILLIS * 2)
        storage.setDateOfUpdate(twoDaysAgo)

        val storedDate = storage.getDaysSinceUpdate()
        Assert.assertEquals(2, storedDate)
    }

    @Test
    fun testGettingAndSettingHasBeenCleared() {
        val hasBeenCleared = true
        storage.setWhatsNewHasBeenCleared(hasBeenCleared)

        val storedHasBeenCleared = storage.getWhatsNewHasBeenCleared()
        Assert.assertEquals(hasBeenCleared, storedHasBeenCleared)
    }

    companion object {
        const val DAY_IN_MILLIS = 3600 * 1000 * 24
    }
}

fun Settings.clear() {
    preferences.clearAndCommit()
}
