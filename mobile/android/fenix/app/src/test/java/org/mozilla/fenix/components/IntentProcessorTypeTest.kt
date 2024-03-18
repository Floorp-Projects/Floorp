/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Intent
import android.content.Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY
import io.mockk.every
import io.mockk.mockk
import mozilla.components.feature.intent.processing.IntentProcessor
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.customtabs.ExternalAppBrowserActivity
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class IntentProcessorTypeTest {
    @Before
    fun setup() {
        every { testContext.components.intentProcessors } returns mockk(relaxed = true)
    }

    @Test
    fun `should open intent with flag launched from history`() {
        val intent: Intent = mockk()
        every { intent.flags } returns FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY

        assertTrue(IntentProcessorType.EXTERNAL_APP.shouldOpenToBrowser(intent))
        assertFalse(IntentProcessorType.NEW_TAB.shouldOpenToBrowser(intent))
        assertFalse(IntentProcessorType.OTHER.shouldOpenToBrowser(intent))
    }

    @Test
    fun `should open intent without flag launched from history`() {
        val intent: Intent = mockk()
        every { intent.flags } returns 0

        assertTrue(IntentProcessorType.EXTERNAL_APP.shouldOpenToBrowser(intent))
        assertTrue(IntentProcessorType.NEW_TAB.shouldOpenToBrowser(intent))
        assertFalse(IntentProcessorType.OTHER.shouldOpenToBrowser(intent))
    }

    @Test
    fun `get type for normal intent processor`() {
        val processor = testContext.components.intentProcessors.intentProcessor
        val type = testContext.components.intentProcessors.getType(processor)

        assertEquals(IntentProcessorType.NEW_TAB, type)
        assertEquals(HomeActivity::class.java.name, type.activityClassName)
    }

    @Test
    fun `get type for private intent processor`() {
        val processor = testContext.components.intentProcessors.privateIntentProcessor
        val type = testContext.components.intentProcessors.getType(processor)

        assertEquals(IntentProcessorType.NEW_TAB, type)
        assertEquals(HomeActivity::class.java.name, type.activityClassName)
    }

    @Test
    fun `get type for web notifications intent processor`() {
        val processor = testContext.components.intentProcessors.webNotificationsIntentProcessor
        val type = testContext.components.intentProcessors.getType(processor)

        assertEquals(IntentProcessorType.NEW_TAB, type)
        assertEquals(HomeActivity::class.java.name, type.activityClassName)
    }

    @Test
    fun `get type for custom tab intent processor`() {
        val processor = testContext.components.intentProcessors.customTabIntentProcessor
        val type = testContext.components.intentProcessors.getType(processor)

        assertEquals(IntentProcessorType.EXTERNAL_APP, type)
        assertEquals(ExternalAppBrowserActivity::class.java.name, type.activityClassName)
    }

    @Test
    fun `get type for private custom tab intent processor`() {
        every { testContext.components.intentProcessors } returns mockk(relaxed = true)
        val processor = testContext.components.intentProcessors.privateCustomTabIntentProcessor
        val type = testContext.components.intentProcessors.getType(processor)

        assertEquals(IntentProcessorType.EXTERNAL_APP, type)
        assertEquals(ExternalAppBrowserActivity::class.java.name, type.activityClassName)
    }

    @Test
    fun `get type for TWA intent processor`() {
        val processor = testContext.components.intentProcessors.privateCustomTabIntentProcessor
        val type = testContext.components.intentProcessors.getType(processor)

        assertEquals(IntentProcessorType.EXTERNAL_APP, type)
        assertEquals(ExternalAppBrowserActivity::class.java.name, type.activityClassName)
    }

    @Test
    fun `get type for PWA intent processor`() {
        val processor = testContext.components.intentProcessors.privateCustomTabIntentProcessor
        val type = testContext.components.intentProcessors.getType(processor)

        assertEquals(IntentProcessorType.EXTERNAL_APP, type)
        assertEquals(ExternalAppBrowserActivity::class.java.name, type.activityClassName)
    }

    @Test
    fun `get type for Deeplink intent processor`() {
        val processor = testContext.components.intentProcessors.externalDeepLinkIntentProcessor
        val type = testContext.components.intentProcessors.getType(processor)

        assertEquals(IntentProcessorType.EXTERNAL_DEEPLINK, type)
        assertEquals(HomeActivity::class.java.name, type.activityClassName)
    }

    @Test
    fun `get type for generic intent processor`() {
        val processor = object : IntentProcessor {
            override fun process(intent: Intent) = true
        }
        val type = testContext.components.intentProcessors.getType(processor)

        assertEquals(IntentProcessorType.OTHER, type)
        assertEquals(HomeActivity::class.java.name, type.activityClassName)
    }
}
