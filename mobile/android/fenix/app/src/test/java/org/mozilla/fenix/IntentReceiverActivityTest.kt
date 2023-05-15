/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.app.Activity
import android.content.Intent
import android.content.Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY
import android.net.Uri
import io.mockk.Runs
import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.spyk
import io.mockk.unmockkStatic
import io.mockk.verify
import kotlinx.coroutines.test.runTest
import mozilla.components.feature.intent.processing.IntentProcessor
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.components.IntentProcessorType
import org.mozilla.fenix.components.IntentProcessors
import org.mozilla.fenix.customtabs.ExternalAppBrowserActivity
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.helpers.perf.TestStrictModeManager
import org.mozilla.fenix.shortcut.NewTabShortcutIntentProcessor
import org.mozilla.fenix.utils.Settings
import org.robolectric.Robolectric
import org.robolectric.Shadows.shadowOf

@RunWith(FenixRobolectricTestRunner::class)
class IntentReceiverActivityTest {

    private lateinit var settings: Settings
    private lateinit var intentProcessors: IntentProcessors

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @Before
    fun setup() {
        mockkStatic("org.mozilla.fenix.ext.ContextKt")
        settings = mockk()
        intentProcessors = mockk()

        every { settings.openLinksInAPrivateTab } returns false
        every { intentProcessors.intentProcessor } returns mockIntentProcessor()
        every { intentProcessors.privateIntentProcessor } returns mockIntentProcessor()
        every { intentProcessors.customTabIntentProcessor } returns mockIntentProcessor()
        every { intentProcessors.privateCustomTabIntentProcessor } returns mockIntentProcessor()
        every { intentProcessors.externalAppIntentProcessors } returns emptyList()
        every { intentProcessors.fennecPageShortcutIntentProcessor } returns mockIntentProcessor()
        every { intentProcessors.externalDeepLinkIntentProcessor } returns mockIntentProcessor()
        every { intentProcessors.webNotificationsIntentProcessor } returns mockIntentProcessor()

        coEvery { intentProcessors.intentProcessor.process(any()) } returns true
    }

    @After
    fun teardown() {
        unmockkStatic("org.mozilla.fenix.ext.ContextKt")
    }

    @Test
    fun `process intent with flag launched from history`() = runTest {
        val intent = Intent()
        intent.flags = FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY
        assertNull(Events.openedLink.testGetValue())

        val activity = Robolectric.buildActivity(IntentReceiverActivity::class.java, intent).get()
        attachMocks(activity)
        activity.processIntent(intent)

        val shadow = shadowOf(activity)
        val actualIntent = shadow.peekNextStartedActivity()

        assertNotNull(Events.openedLink.testGetValue())
        assertEquals(HomeActivity::class.java.name, actualIntent.component?.className)
        assertEquals(true, actualIntent.flags == FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY)
    }

    @Test
    fun `GIVEN a deeplink intent WHEN processing the intent THEN add the className HomeActivity`() =
        runTest {
            val uri = Uri.parse(BuildConfig.DEEP_LINK_SCHEME + "://settings_wallpapers")
            val intent = Intent("", uri)
            assertNull(Events.openedLink.testGetValue())

            coEvery { intentProcessors.intentProcessor.process(any()) } returns false
            coEvery { intentProcessors.externalDeepLinkIntentProcessor.process(any()) } returns true

            val activity =
                Robolectric.buildActivity(IntentReceiverActivity::class.java, intent).get()
            attachMocks(activity)
            activity.processIntent(intent)

            val shadow = shadowOf(activity)
            val actualIntent = shadow.peekNextStartedActivity()

            assertNotNull(Events.openedLink.testGetValue())
            assertEquals(HomeActivity::class.java.name, actualIntent.component?.className)
        }

    @Test
    fun `process intent with action OPEN_PRIVATE_TAB`() = runTest {
        val intent = Intent()
        intent.action = NewTabShortcutIntentProcessor.ACTION_OPEN_PRIVATE_TAB
        assertNull(Events.openedLink.testGetValue())

        coEvery { intentProcessors.intentProcessor.process(intent) } returns false
        coEvery { intentProcessors.customTabIntentProcessor.process(intent) } returns false
        val activity = Robolectric.buildActivity(IntentReceiverActivity::class.java, intent).get()
        attachMocks(activity)
        activity.processIntent(intent)

        val shadow = shadowOf(activity)
        val actualIntent = shadow.peekNextStartedActivity()

        assertNotNull(Events.openedLink.testGetValue())
        assertEquals(HomeActivity::class.java.name, actualIntent.component?.className)
        assertEquals(true, actualIntent.getBooleanExtra(HomeActivity.PRIVATE_BROWSING_MODE, false))
        assertEquals(false, actualIntent.getBooleanExtra(HomeActivity.OPEN_TO_BROWSER, true))
    }

    @Test
    fun `process intent with action OPEN_TAB`() = runTest {
        assertNull(Events.openedLink.testGetValue())
        val intent = Intent()
        intent.action = NewTabShortcutIntentProcessor.ACTION_OPEN_TAB

        val activity = Robolectric.buildActivity(IntentReceiverActivity::class.java, intent).get()
        attachMocks(activity)
        activity.processIntent(intent)

        val shadow = shadowOf(activity)
        val actualIntent = shadow.peekNextStartedActivity()

        assertEquals(HomeActivity::class.java.name, actualIntent.component?.className)
        assertEquals(false, actualIntent.getBooleanExtra(HomeActivity.PRIVATE_BROWSING_MODE, false))
        assertNotNull(Events.openedLink.testGetValue())
    }

    @Test
    fun `process intent starts Activity`() = runTest {
        assertNull(Events.openedLink.testGetValue())
        val intent = Intent()
        val activity = Robolectric.buildActivity(IntentReceiverActivity::class.java, intent).get()
        attachMocks(activity)
        activity.processIntent(intent)

        val shadow = shadowOf(activity)
        val actualIntent = shadow.peekNextStartedActivity()

        assertEquals(HomeActivity::class.java.name, actualIntent.component?.className)
        assertEquals(true, actualIntent.getBooleanExtra(HomeActivity.OPEN_TO_BROWSER, true))
        assertNotNull(Events.openedLink.testGetValue())
    }

    @Test
    fun `process intent with launchLinksInPrivateTab set to true`() = runTest {
        assertNull(Events.openedLink.testGetValue())

        every { settings.openLinksInAPrivateTab } returns true

        coEvery { intentProcessors.intentProcessor.process(any()) } returns false
        coEvery { intentProcessors.privateIntentProcessor.process(any()) } returns true

        val intent = Intent()
        val activity = Robolectric.buildActivity(IntentReceiverActivity::class.java, intent).get()
        attachMocks(activity)
        activity.processIntent(intent)

        val shadow = shadowOf(activity)
        val actualIntent = shadow.peekNextStartedActivity()

        val normalProcessor = intentProcessors.intentProcessor
        verify(exactly = 0) { normalProcessor.process(intent) }
        verify { intentProcessors.privateIntentProcessor.process(intent) }
        assertEquals(HomeActivity::class.java.name, actualIntent.component?.className)
        assertTrue(actualIntent.getBooleanExtra(HomeActivity.PRIVATE_BROWSING_MODE, false))
        assertNotNull(Events.openedLink.testGetValue())
    }

    @Test
    fun `process intent with launchLinksInPrivateTab set to false`() = runTest {
        assertNull(Events.openedLink.testGetValue())
        val intent = Intent()

        val activity = Robolectric.buildActivity(IntentReceiverActivity::class.java, intent).get()
        attachMocks(activity)
        activity.processIntent(intent)

        coVerify(exactly = 0) { intentProcessors.privateIntentProcessor.process(intent) }
        coVerify { intentProcessors.intentProcessor.process(intent) }
        assertNotNull(Events.openedLink.testGetValue())
    }

    @Test
    fun `process intent with launchLinksInPrivateTab set to false but with external flag`() = runTest {
        assertNull(Events.openedLink.testGetValue())

        coEvery { intentProcessors.intentProcessor.process(any()) } returns false
        coEvery { intentProcessors.privateIntentProcessor.process(any()) } returns true

        val intent = Intent()
        intent.putExtra(HomeActivity.PRIVATE_BROWSING_MODE, true)

        val activity = Robolectric.buildActivity(IntentReceiverActivity::class.java, intent).get()
        attachMocks(activity)
        activity.processIntent(intent)

        val shadow = shadowOf(activity)
        val actualIntent = shadow.peekNextStartedActivity()

        val normalProcessor = intentProcessors.intentProcessor
        verify(exactly = 0) { normalProcessor.process(intent) }
        verify { intentProcessors.privateIntentProcessor.process(intent) }
        assertEquals(HomeActivity::class.java.name, actualIntent.component?.className)
        assertTrue(actualIntent.getBooleanExtra(HomeActivity.PRIVATE_BROWSING_MODE, false))
        assertNotNull(Events.openedLink.testGetValue())
    }

    @Test
    fun `process custom tab intent`() = runTest {
        assertNull(Events.openedLink.testGetValue())
        val intent = Intent()
        coEvery { intentProcessors.intentProcessor.process(intent) } returns false
        coEvery { intentProcessors.customTabIntentProcessor.process(intent) } returns true
        assertNull(Events.openedLink.testGetValue())

        val activity = Robolectric.buildActivity(IntentReceiverActivity::class.java, intent).get()
        attachMocks(activity)
        activity.processIntent(intent)

        coVerify(exactly = 0) { intentProcessors.privateCustomTabIntentProcessor.process(intent) }
        coVerify { intentProcessors.customTabIntentProcessor.process(intent) }

        assertEquals(ExternalAppBrowserActivity::class.java.name, intent.component!!.className)
        assertTrue(intent.getBooleanExtra(HomeActivity.OPEN_TO_BROWSER, false))
        assertNotNull(Events.openedLink.testGetValue())
    }

    @Test
    fun `process private custom tab intent`() = runTest {
        assertNull(Events.openedLink.testGetValue())
        every { settings.openLinksInAPrivateTab } returns true

        val intent = Intent()
        coEvery { intentProcessors.privateCustomTabIntentProcessor.process(intent) } returns true

        val activity = Robolectric.buildActivity(IntentReceiverActivity::class.java, intent).get()
        attachMocks(activity)
        activity.processIntent(intent)

        val normalProcessor = intentProcessors.customTabIntentProcessor
        coVerify(exactly = 0) { normalProcessor.process(intent) }
        coVerify { intentProcessors.privateCustomTabIntentProcessor.process(intent) }

        assertEquals(ExternalAppBrowserActivity::class.java.name, intent.component!!.className)
        assertTrue(intent.getBooleanExtra(HomeActivity.OPEN_TO_BROWSER, false))
        assertNotNull(Events.openedLink.testGetValue())
    }

    @Test
    fun `process web notifications click intent`() {
        val intent = Intent()
        every { intentProcessors.webNotificationsIntentProcessor.process(intent) } returns true
        val activity = spyk(Robolectric.buildActivity(IntentReceiverActivity::class.java, intent).get())
        attachMocks(activity)
        every { activity.launch(any(), any()) } just Runs
        activity.processIntent(intent)

        verify { intentProcessors.webNotificationsIntentProcessor.process(intent) }
        verify { activity.launch(intent, IntentProcessorType.NEW_TAB) }
    }

    private fun attachMocks(activity: Activity) {
        every { activity.settings() } returns settings
        every { activity.components.analytics } returns mockk(relaxed = true)
        every { activity.components.intentProcessors } returns intentProcessors
        every { activity.components.strictMode } returns TestStrictModeManager()
    }

    private inline fun <reified T : IntentProcessor> mockIntentProcessor(): T {
        return mockk {
            coEvery { process(any()) } returns false
        }
    }
}
