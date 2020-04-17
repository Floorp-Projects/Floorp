/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Intent
import android.content.Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.mozilla.fenix.customtabs.ExternalAppBrowserActivity
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.shortcut.NewTabShortcutIntentProcessor
import org.robolectric.Robolectric
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.robolectric.Shadows.shadowOf

@ExperimentalCoroutinesApi
@RunWith(FenixRobolectricTestRunner::class)
class IntentReceiverActivityTest {

    @Test
    fun `process intent with flag launched from history`() = runBlockingTest {
        testContext.settings().openLinksInAPrivateTab = false

        val intent = Intent()
        intent.flags = FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY

        `when`(testContext.components.intentProcessors.intentProcessor.process(intent)).thenReturn(true)
        `when`(testContext.components.intentProcessors.customTabIntentProcessor.process(intent)).thenReturn(false)
        val activity = Robolectric.buildActivity(IntentReceiverActivity::class.java, intent).get()
        activity.processIntent(intent)

        val shadow = shadowOf(activity)
        val actualIntent = shadow.peekNextStartedActivity()

        assertEquals(HomeActivity::class.java.name, actualIntent.component?.className)
        assertEquals(true, actualIntent.flags == FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY)
    }

    @Test
    fun `process intent with action OPEN_PRIVATE_TAB`() = runBlockingTest {
        testContext.settings().openLinksInAPrivateTab = false

        val intent = Intent()
        intent.action = NewTabShortcutIntentProcessor.ACTION_OPEN_PRIVATE_TAB

        `when`(testContext.components.intentProcessors.intentProcessor.process(intent)).thenReturn(false)
        `when`(testContext.components.intentProcessors.customTabIntentProcessor.process(intent)).thenReturn(false)
        val activity = Robolectric.buildActivity(IntentReceiverActivity::class.java, intent).get()
        activity.processIntent(intent)

        val shadow = shadowOf(activity)
        val actualIntent = shadow.peekNextStartedActivity()

        assertEquals(HomeActivity::class.java.name, actualIntent.component?.className)
        assertEquals(true, actualIntent.getBooleanExtra(HomeActivity.PRIVATE_BROWSING_MODE, false))
        assertEquals(false, actualIntent.getBooleanExtra(HomeActivity.OPEN_TO_BROWSER, true))
    }

    @Test
    fun `process intent with action OPEN_TAB`() = runBlockingTest {
        testContext.settings().openLinksInAPrivateTab = false

        val intent = Intent()
        intent.action = NewTabShortcutIntentProcessor.ACTION_OPEN_TAB

        `when`(testContext.components.intentProcessors.intentProcessor.process(intent)).thenReturn(true)
        `when`(testContext.components.intentProcessors.customTabIntentProcessor.process(intent)).thenReturn(false)
        val activity = Robolectric.buildActivity(IntentReceiverActivity::class.java, intent).get()
        activity.processIntent(intent)

        val shadow = shadowOf(activity)
        val actualIntent = shadow.peekNextStartedActivity()

        assertEquals(HomeActivity::class.java.name, actualIntent.component?.className)
        assertEquals(false, actualIntent.hasExtra(HomeActivity.PRIVATE_BROWSING_MODE))
        assertEquals(false, actualIntent.getBooleanExtra(HomeActivity.OPEN_TO_BROWSER, true))
    }

    @Test
    fun `process intent starts Activity`() = runBlockingTest {
        testContext.settings().openLinksInAPrivateTab = false

        val intent = Intent()
        `when`(testContext.components.intentProcessors.intentProcessor.process(intent)).thenReturn(true)
        `when`(testContext.components.intentProcessors.customTabIntentProcessor.process(intent)).thenReturn(false)
        val activity = Robolectric.buildActivity(IntentReceiverActivity::class.java, intent).get()
        activity.processIntent(intent)

        val shadow = shadowOf(activity)
        val actualIntent = shadow.peekNextStartedActivity()

        assertEquals(HomeActivity::class.java.name, actualIntent.component?.className)
        assertEquals(false, actualIntent.getBooleanExtra(HomeActivity.OPEN_TO_BROWSER, true))
    }

    @Test
    fun `process intent with launchLinksInPrivateTab set to true`() = runBlockingTest {
        testContext.settings().openLinksInAPrivateTab = true

        val intent = Intent()
        `when`(testContext.components.intentProcessors.privateIntentProcessor.process(intent)).thenReturn(true)
        `when`(testContext.components.intentProcessors.privateCustomTabIntentProcessor.process(intent)).thenReturn(false)
        val activity = Robolectric.buildActivity(IntentReceiverActivity::class.java, intent).get()
        activity.processIntent(intent)

        // Not using mockk here because process is a suspend function
        // and mockito makes this easier to read.
        verify(testContext.components.intentProcessors.intentProcessor, never()).process(intent)
        verify(testContext.components.intentProcessors.privateIntentProcessor).process(intent)
    }

    @Test
    fun `process intent with launchLinksInPrivateTab set to false`() = runBlockingTest {
        testContext.settings().openLinksInAPrivateTab = false

        val intent = Intent()
        `when`(testContext.components.intentProcessors.intentProcessor.process(intent)).thenReturn(true)
        `when`(testContext.components.intentProcessors.customTabIntentProcessor.process(intent)).thenReturn(false)

        val activity = Robolectric.buildActivity(IntentReceiverActivity::class.java, intent).get()
        activity.processIntent(intent)

        // Not using mockk here because process is a suspend function
        // and mockito makes this easier to read.
        verify(testContext.components.intentProcessors.privateIntentProcessor, never()).process(intent)
        verify(testContext.components.intentProcessors.intentProcessor).process(intent)
    }

    @Test
    fun `process custom tab intent`() = runBlockingTest {
        testContext.settings().openLinksInAPrivateTab = false

        val intent = Intent()
        `when`(testContext.components.intentProcessors.customTabIntentProcessor.process(intent)).thenReturn(true)

        val activity = Robolectric.buildActivity(IntentReceiverActivity::class.java, intent).get()
        activity.processIntent(intent)

        // Not using mockk here because process is a suspend function
        // and mockito makes this easier to read.
        verify(testContext.components.intentProcessors.privateIntentProcessor, never()).process(intent)
        verify(testContext.components.intentProcessors.customTabIntentProcessor).process(intent)

        assertEquals(ExternalAppBrowserActivity::class.java.name, intent.component!!.className)
        assertTrue(intent.getBooleanExtra(HomeActivity.OPEN_TO_BROWSER, false))
    }

    @Test
    fun `process private custom tab intent`() = runBlockingTest {
        testContext.settings().openLinksInAPrivateTab = true

        val intent = Intent()
        `when`(testContext.components.intentProcessors.privateCustomTabIntentProcessor.process(intent)).thenReturn(true)

        val activity = Robolectric.buildActivity(IntentReceiverActivity::class.java, intent).get()
        activity.processIntent(intent)

        // Not using mockk here because process is a suspend function
        // and mockito makes this easier to read.
        verify(testContext.components.intentProcessors.intentProcessor, never()).process(intent)
        verify(testContext.components.intentProcessors.privateCustomTabIntentProcessor).process(intent)

        assertEquals(ExternalAppBrowserActivity::class.java.name, intent.component!!.className)
        assertTrue(intent.getBooleanExtra(HomeActivity.OPEN_TO_BROWSER, false))
    }
}
