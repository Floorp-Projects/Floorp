/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Intent
import android.speech.RecognizerIntent.ACTION_RECOGNIZE_SPEECH
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals

import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.eq
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import org.robolectric.annotation.Config
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner

@ObsoleteCoroutinesApi
@ExperimentalCoroutinesApi
@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)
class IntentReceiverActivityTest {

    @Test
    fun `process intent with alwaysOpenInPrivateMode set to true`() {
        runBlockingTest {
            testContext.settings.alwaysOpenInPrivateMode = true

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
    fun `process intent with alwaysOpenInPrivateMode set to false`() {
        runBlockingTest {
            testContext.settings.alwaysOpenInPrivateMode = false

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

    @Test
    fun `process intent with speech processing set to true`() {
        runBlockingTest {
            val intent = Intent()
            intent.putExtra(IntentReceiverActivity.SPEECH_PROCESSING, true)

            val activity = spy(Robolectric.buildActivity(IntentReceiverActivity::class.java, intent).get())
            activity.processIntent(intent)

            val speechIntent = argumentCaptor<Intent>()

            // Not using mockk here because process is a suspend function
            // and mockito makes this easier to read.
            verify(testContext.components.intentProcessors.privateIntentProcessor, never()).process(intent)
            verify(testContext.components.intentProcessors.intentProcessor, never()).process(intent)
            verify(activity).startActivityForResult(speechIntent.capture(), eq(IntentReceiverActivity.SPEECH_REQUEST_CODE))
            assertEquals(ACTION_RECOGNIZE_SPEECH, speechIntent.value.action)
        }
    }
}
