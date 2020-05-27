/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.intent

import android.content.Intent
import androidx.navigation.NavController
import io.mockk.Called
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import io.mockk.verifyOrder
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.media.service.AbstractMediaService
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class OpenSpecificTabIntentProcessorTest {
    private lateinit var activity: HomeActivity
    private lateinit var navController: NavController
    private lateinit var out: Intent
    private lateinit var processor: OpenSpecificTabIntentProcessor

    @Before
    fun setup() {
        activity = mockk(relaxed = true)
        navController = mockk(relaxed = true)
        out = mockk()
        processor = OpenSpecificTabIntentProcessor(activity)
    }

    @Test
    fun `GIVEN a blank intent, WHEN it is processed, THEN nothing should happen`() {
        assertFalse(processor.process(Intent(), navController, out))

        verify { activity wasNot Called }
        verify { navController wasNot Called }
        verify { out wasNot Called }
    }

    @Test
    fun `GIVEN an intent with wrong action, WHEN it is processed, THEN nothing should happen`() {
        val intent = Intent().apply {
            action = TEST_WRONG_ACTION
        }

        assertFalse(processor.process(intent, navController, out))

        verify { activity wasNot Called }
        verify { navController wasNot Called }
        verify { out wasNot Called }
    }

    @Test
    fun `GIVEN an intent with null extra string, WHEN it is processed, THEN openToBrowser should not be called`() {
        val intent = Intent().apply {
            action = AbstractMediaService.Companion.ACTION_SWITCH_TAB
        }
        every { activity.components.core.sessionManager } returns mockk(relaxed = true)

        assertFalse(processor.process(intent, navController, out))

        verify(exactly = 0) { activity.openToBrowser(BrowserDirection.FromGlobal) }
        verify { navController wasNot Called }
        verify { out wasNot Called }
    }

    @Test
    fun `GIVEN an intent with correct action and extra string, WHEN it is processed, THEN session should be selected and openToBrowser should be called`() {
        val intent = Intent().apply {
            action = AbstractMediaService.Companion.ACTION_SWITCH_TAB
            putExtra(AbstractMediaService.Companion.EXTRA_TAB_ID, TEST_SESSION_ID)
        }
        val sessionManager: SessionManager = mockk(relaxed = true)
        val session: Session = mockk(relaxed = true)
        every { activity.components.core.sessionManager } returns sessionManager
        every { sessionManager.findSessionById(TEST_SESSION_ID) } returns session

        assertTrue(processor.process(intent, navController, out))

        verifyOrder {
            sessionManager.select(session)
            activity.openToBrowser(BrowserDirection.FromGlobal)
        }
        verify { navController wasNot Called }
        verify { out wasNot Called }
    }

    companion object {
        const val TEST_WRONG_ACTION = "test-action"
        const val TEST_SESSION_ID = "test-session-id"
    }
}
