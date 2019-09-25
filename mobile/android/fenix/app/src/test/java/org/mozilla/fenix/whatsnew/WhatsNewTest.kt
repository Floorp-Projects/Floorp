package org.mozilla.fenix.whatsnew

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.TestApplication
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.utils.Settings
import org.robolectric.annotation.Config

@ObsoleteCoroutinesApi
@RunWith(AndroidJUnit4::class)
@Config(application = TestApplication::class)
class WhatsNewTest {
    private lateinit var storage: SharedPreferenceWhatsNewStorage
    private lateinit var settings: Settings

    @Before
    fun setup() {
        storage = SharedPreferenceWhatsNewStorage(testContext)
        settings = testContext.settings().apply(Settings::clear)
        WhatsNew.wasUpdatedRecently = null
    }

    @Test
    fun `should highlight after fresh install`() {
        assertEquals(true, WhatsNew.shouldHighlightWhatsNew(testContext))
    }

    @Test
    fun `should highlight if new version has been installed less than 3 days`() {
        storage.setVersion(WhatsNewVersion("1.0"))
        storage.setDateOfUpdate(System.currentTimeMillis() - WhatsNewStorageTest.DAY_IN_MILLIS)

        assertEquals(true, WhatsNew.shouldHighlightWhatsNew(WhatsNewVersion("2.0"), storage))
    }

    @Test
    fun `should not highlight if new version has been installed more than 3 days`() {
        storage.setVersion(WhatsNewVersion("1.0"))
        assertEquals(true, WhatsNew.shouldHighlightWhatsNew(WhatsNewVersion("2.0"), storage))

        // Simulate an app "restart" by resetting `wasUpdatedRecently`
        WhatsNew.wasUpdatedRecently = null
        // Simulate time passing
        storage.setDateOfUpdate(System.currentTimeMillis() - WhatsNewStorageTest.DAY_IN_MILLIS * 3)

        assertEquals(false, WhatsNew.shouldHighlightWhatsNew(WhatsNewVersion("2.0"), storage))
    }

    @Test
    fun `should not highlight if new version is only a minor update`() {
        storage.setVersion(WhatsNewVersion("1.0"))
        assertEquals(false, WhatsNew.shouldHighlightWhatsNew(WhatsNewVersion("1.0.1"), storage))

        WhatsNew.wasUpdatedRecently = null

        storage.setVersion(WhatsNewVersion("1.0"))
        assertEquals(false, WhatsNew.shouldHighlightWhatsNew(WhatsNewVersion("1.1"), storage))
    }

    @Test
    fun `should highlight if new version is a major update`() {
        storage.setVersion(WhatsNewVersion("1.0"))
        assertEquals(true, WhatsNew.shouldHighlightWhatsNew(WhatsNewVersion("2.0"), storage))

        WhatsNew.wasUpdatedRecently = null

        storage.setVersion(WhatsNewVersion("1.2"))
        assertEquals(true, WhatsNew.shouldHighlightWhatsNew(WhatsNewVersion("2.0"), storage))

        WhatsNew.wasUpdatedRecently = null

        storage.setVersion(WhatsNewVersion("3.2"))
        assertEquals(true, WhatsNew.shouldHighlightWhatsNew(WhatsNewVersion("4.2"), storage))
    }

    @Test
    fun `should not highlight after user viewed what's new`() {
        assertEquals(true, WhatsNew.shouldHighlightWhatsNew(testContext))

        WhatsNew.userViewedWhatsNew(testContext)

        assertEquals(false, WhatsNew.shouldHighlightWhatsNew(testContext))
    }
}
