/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customtabs

import android.content.Intent
import android.os.Bundle
import androidx.navigation.NavDirections
import io.mockk.Called
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.intent.ext.putSessionId
import mozilla.components.support.utils.toSafeIntent
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getIntentSource
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class ExternalAppBrowserActivityTest {

    @Test
    fun getIntentSource() {
        val activity = ExternalAppBrowserActivity()

        val launcherIntent = Intent(Intent.ACTION_MAIN).apply {
            addCategory(Intent.CATEGORY_LAUNCHER)
        }.toSafeIntent()
        assertEquals("CUSTOM_TAB", activity.getIntentSource(launcherIntent))

        val viewIntent = Intent(Intent.ACTION_VIEW).toSafeIntent()
        assertEquals("CUSTOM_TAB", activity.getIntentSource(viewIntent))

        val otherIntent = Intent().toSafeIntent()
        assertEquals("CUSTOM_TAB", activity.getIntentSource(otherIntent))
    }

    @Test
    fun `navigateToBrowserOnColdStart does nothing for external app browser activity`() {
        val activity = spyk(ExternalAppBrowserActivity())

        val settings: Settings = mockk()
        every { settings.shouldReturnToBrowser } returns true
        every { activity.components.settings.shouldReturnToBrowser } returns true
        every { activity.openToBrowser(any(), any()) } returns Unit

        activity.navigateToBrowserOnColdStart()

        verify(exactly = 0) { activity.openToBrowser(BrowserDirection.FromGlobal, null) }
    }

    @Test
    fun `handleNewIntent does nothing for external app browser activity`() {
        val activity = spyk(ExternalAppBrowserActivity())
        val intent: Intent = mockk(relaxed = true)

        activity.handleNewIntent(intent)
        verify { intent wasNot Called }
    }

    @Test
    fun `getNavDirections finishes activity if session ID is null`() {
        val activity = spyk(
            object : ExternalAppBrowserActivity() {
                public override fun getNavDirections(
                    from: BrowserDirection,
                    customTabSessionId: String?,
                ): NavDirections? {
                    return super.getNavDirections(from, customTabSessionId)
                }

                override fun getIntent(): Intent {
                    val intent: Intent = mockk()
                    val bundle: Bundle = mockk()
                    every { bundle.getString(any()) } returns ""
                    every { intent.extras } returns bundle
                    every { intent.getBooleanExtra(any(), any()) } returns false
                    return intent
                }
            },
        )

        var directions = activity.getNavDirections(BrowserDirection.FromGlobal, "id")
        assertNotNull(directions)
        verify(exactly = 0) { activity.finishAndRemoveTask() }

        directions = activity.getNavDirections(BrowserDirection.FromGlobal, null)
        assertNull(directions)
        verify { activity.finishAndRemoveTask() }
    }

    @Test
    fun `GIVEN intent isSandboxCustomTab is true WHEN getNavDirections called THEN actionGlobalExternalAppBrowser isSandboxCustomTab is true`() {
        val activity = spyk(
            object : ExternalAppBrowserActivity() {
                public override fun getNavDirections(
                    from: BrowserDirection,
                    customTabSessionId: String?,
                ): NavDirections? {
                    return super.getNavDirections(from, customTabSessionId)
                }

                override fun getIntent(): Intent {
                    val intent: Intent = mockk()
                    val bundle: Bundle = mockk()
                    every { bundle.getString(any()) } returns ""
                    every { intent.getBooleanExtra(any(), any()) } returns true
                    every { intent.extras } returns bundle
                    return intent
                }
            },
        )

        val customTabSessionId = "id"
        val directions = activity.getNavDirections(BrowserDirection.FromGlobal, customTabSessionId)
        assertNotNull(directions)
        verify(exactly = 0) { activity.finishAndRemoveTask() }

        val expected = NavGraphDirections.actionGlobalExternalAppBrowser(
            activeSessionId = customTabSessionId,
            webAppManifest = null,
            isSandboxCustomTab = true,
        )
        assertEquals(expected, directions)
    }

    @Test
    fun `GIVEN intent isSandboxCustomTab is false WHEN getNavDirections called THEN actionGlobalExternalAppBrowser isSandboxCustomTab is false`() {
        val activity = spyk(
            object : ExternalAppBrowserActivity() {
                public override fun getNavDirections(
                    from: BrowserDirection,
                    customTabSessionId: String?,
                ): NavDirections? {
                    return super.getNavDirections(from, customTabSessionId)
                }

                override fun getIntent(): Intent {
                    val intent: Intent = mockk()
                    val bundle: Bundle = mockk()
                    every { bundle.getString(any()) } returns ""
                    every { intent.getBooleanExtra(any(), any()) } returns false
                    every { intent.extras } returns bundle
                    return intent
                }
            },
        )

        val customTabSessionId = "id"
        val directions = activity.getNavDirections(BrowserDirection.FromGlobal, customTabSessionId)
        assertNotNull(directions)
        verify(exactly = 0) { activity.finishAndRemoveTask() }

        val expected = NavGraphDirections.actionGlobalExternalAppBrowser(
            activeSessionId = customTabSessionId,
            webAppManifest = null,
            isSandboxCustomTab = false,
        )
        assertEquals(expected, directions)
    }

    @Test
    fun `ExternalAppBrowserActivity with matching external tab`() {
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(
                    createCustomTab(
                        url = "https://www.mozilla.org",
                        id = "mozilla",
                    ),
                ),
            ),
        )

        val intent = Intent(Intent.ACTION_VIEW).apply { putSessionId("mozilla") }

        val activity = spyk(ExternalAppBrowserActivity())
        every { activity.components.core.store } returns store
        every { activity.intent } returns intent

        assertTrue(activity.hasExternalTab())

        assertEquals("mozilla", activity.getExternalTabId())

        val tab = activity.getExternalTab()
        assertNotNull(tab!!)
        assertEquals("https://www.mozilla.org", tab.content.url)
    }

    @Test
    fun `ExternalAppBrowserActivity without matching external tab`() {
        val store = BrowserStore()

        val intent = Intent(Intent.ACTION_VIEW).apply { putSessionId("mozilla") }

        val activity = spyk(ExternalAppBrowserActivity())
        every { activity.components.core.store } returns store
        every { activity.intent } returns intent

        assertFalse(activity.hasExternalTab())
        assertEquals("mozilla", activity.getExternalTabId())
        assertNull(activity.getExternalTab())
    }

    @Test
    fun `ExternalAppBrowserActivity with matching regular tab`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(
                        url = "https://www.mozilla.org",
                        id = "mozilla",
                    ),
                ),
            ),
        )

        val intent = Intent(Intent.ACTION_VIEW).apply { putSessionId("mozilla") }

        val activity = spyk(ExternalAppBrowserActivity())
        every { activity.components.core.store } returns store
        every { activity.intent } returns intent

        // Even though we have a matching regular tab we do not care about it in ExternalAppBrowserActivity
        assertFalse(activity.hasExternalTab())
        assertEquals("mozilla", activity.getExternalTabId())
        assertNull(activity.getExternalTab())
    }
}
