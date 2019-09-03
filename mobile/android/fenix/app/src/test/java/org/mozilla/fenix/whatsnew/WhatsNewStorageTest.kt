package org.mozilla.fenix.whatsnew

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.TestApplication
import org.mozilla.fenix.ext.clearAndCommit
import org.mozilla.fenix.utils.Settings
import org.robolectric.annotation.Config

@ObsoleteCoroutinesApi
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

private fun Settings.clear() {
    preferences.clearAndCommit()
}
