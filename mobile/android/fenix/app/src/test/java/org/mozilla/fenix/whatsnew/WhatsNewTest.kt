/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.whatsnew

import androidx.preference.PreferenceManager
import io.mockk.every
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.ext.clearAndCommit
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.helpers.perf.TestStrictModeManager

@RunWith(FenixRobolectricTestRunner::class)
class WhatsNewTest {

    private lateinit var storage: SharedPreferenceWhatsNewStorage

    @Before
    fun setup() {
        storage = SharedPreferenceWhatsNewStorage(testContext)
        PreferenceManager.getDefaultSharedPreferences(testContext).clearAndCommit()
        WhatsNew.wasUpdatedRecently = null
    }

    @Test
    fun `should highlight after fresh install`() {
        every { testContext.components.strictMode } returns TestStrictModeManager()
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
        every { testContext.components.strictMode } returns TestStrictModeManager()
        assertEquals(true, WhatsNew.shouldHighlightWhatsNew(testContext))

        WhatsNew.userViewedWhatsNew(testContext)

        assertEquals(false, WhatsNew.shouldHighlightWhatsNew(testContext))
    }
}
