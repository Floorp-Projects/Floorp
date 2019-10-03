/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Intent
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.support.test.robolectric.testContext
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@ObsoleteCoroutinesApi
@ExperimentalCoroutinesApi
@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)
class IntentReceiverActivityTest {

    @Test
    fun `process intent with launchLinksInPrivateTab set to true`() {
        runBlockingTest {
            testContext.settings().openLinksInAPrivateTab = true

            val intent = Intent()
            `when`(testContext.components.intentProcessors.privateIntentProcessor.process(intent)).thenReturn(true)
            val activity = Robolectric.buildActivity(IntentReceiverActivity::class.java, intent).get()
            activity.processIntent(intent)

            // Not using mockk here because process is a suspend function
            // and mockito makes this easier to read.
            verify(testContext.components.intentProcessors.intentProcessor, never()).process(intent)
            verify(testContext.components.intentProcessors.privateIntentProcessor).process(intent)
        }
    }

    @Test
    fun `process intent with launchLinksInPrivateTab set to false`() {
        runBlockingTest {
            testContext.settings().openLinksInAPrivateTab = false

            val intent = Intent()
            `when`(testContext.components.intentProcessors.intentProcessor.process(intent)).thenReturn(true)

            val activity = Robolectric.buildActivity(IntentReceiverActivity::class.java, intent).get()
            activity.processIntent(intent)

            // Not using mockk here because process is a suspend function
            // and mockito makes this easier to read.
            verify(testContext.components.intentProcessors.privateIntentProcessor, never()).process(intent)
            verify(testContext.components.intentProcessors.intentProcessor).process(intent)
        }
    }
}
