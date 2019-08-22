/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.intent

import android.content.Intent
import androidx.core.net.toUri
import androidx.navigation.NavController
import io.mockk.Called
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.ObsoleteCoroutinesApi
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.TestApplication
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@ObsoleteCoroutinesApi
@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)
class DeepLinkIntentProcessorTest {

    private lateinit var activity: HomeActivity
    private lateinit var navController: NavController
    private lateinit var out: Intent
    private lateinit var processor: DeepLinkIntentProcessor

    @Before
    fun setup() {
        activity = mockk(relaxed = true)
        navController = mockk(relaxed = true)
        out = mockk()
        processor = DeepLinkIntentProcessor(activity)
    }

    @Test
    fun `do not process blank intents`() {
        assertFalse(processor.process(Intent(), navController, out))

        verify { activity wasNot Called }
        verify { navController wasNot Called }
        verify { out wasNot Called }
    }

    @Test
    fun `return true if scheme is fenix`() {
        assertTrue(processor.process(testIntent("fenix://test"), navController, out))

        verify { activity wasNot Called }
        verify { navController wasNot Called }
        verify { out wasNot Called }
    }

    @Test
    fun `process home deep link`() {
        assertTrue(processor.process(testIntent("fenix://home"), navController, out))

        verify { activity wasNot Called }
        verify { navController.navigate(NavGraphDirections.actionGlobalHomeFragment()) }
        verify { out wasNot Called }
    }

    @Test
    fun `process settings deep link`() {
        assertTrue(processor.process(testIntent("fenix://settings"), navController, out))

        verify { activity wasNot Called }
        verify { navController.navigate(NavGraphDirections.actionGlobalSettingsFragment()) }
        verify { out wasNot Called }
    }

    @Test
    fun `process turn_on_sync deep link`() {
        assertTrue(processor.process(testIntent("fenix://turn_on_sync"), navController, out))

        verify { activity wasNot Called }
        verify { navController.navigate(NavGraphDirections.actionGlobalTurnOnSync()) }
        verify { out wasNot Called }
    }

    @Test
    fun `process settings_search_engine deep link`() {
        assertTrue(processor.process(testIntent("fenix://settings_search_engine"), navController, out))

        verify { activity wasNot Called }
        verify { navController.navigate(NavGraphDirections.actionGlobalSearchEngineFragment()) }
        verify { out wasNot Called }
    }

    @Test
    fun `process settings_accessibility deep link`() {
        assertTrue(processor.process(testIntent("fenix://settings_accessibility"), navController, out))

        verify { activity wasNot Called }
        verify { navController.navigate(NavGraphDirections.actionGlobalAccessibilityFragment()) }
        verify { out wasNot Called }
    }

    @Test
    fun `process settings_delete_browsing_data deep link`() {
        assertTrue(processor.process(testIntent("fenix://settings_delete_browsing_data"), navController, out))

        verify { activity wasNot Called }
        verify { navController.navigate(NavGraphDirections.actionGlobalDeleteBrowsingDataFragment()) }
        verify { out wasNot Called }
    }

    @Test
    fun `process enable_private_browsing deep link`() {
        assertTrue(processor.process(testIntent("fenix://enable_private_browsing"), navController, out))

        verify { activity.browsingModeManager.mode = BrowsingMode.Private }
        verify { navController.navigate(NavGraphDirections.actionGlobalHomeFragment()) }
        verify { out wasNot Called }
    }

    @Test
    fun `process open deep link`() {
        assertTrue(processor.process(testIntent("fenix://open"), navController, out))

        verify { activity wasNot Called }
        verify { navController wasNot Called }
        verify { out wasNot Called }

        assertTrue(processor.process(testIntent("fenix://open?url=test"), navController, out))

        verify {
            activity.openToBrowserAndLoad(
                "test",
                newTab = true,
                from = BrowserDirection.FromGlobal
            )
        }
        verify { navController wasNot Called }
        verify { out wasNot Called }
    }

    @Test
    fun `process make_default_browser deep link`() {
        assertTrue(processor.process(testIntent("fenix://make_default_browser"), navController, out))

        verify { navController wasNot Called }
        verify { out wasNot Called }
    }

    private fun testIntent(uri: String) = Intent("", uri.toUri())
}
