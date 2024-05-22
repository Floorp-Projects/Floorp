/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.webauthn

import android.app.Activity
import android.content.Intent
import android.content.IntentSender
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.webauthn.WebAuthnFeature.Companion.ACTIVITY_REQUEST_CODE
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.nullable
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.verify

class WebAuthnFeatureTest {
    private lateinit var engine: Engine
    private lateinit var activity: Activity
    private val exitFullScreen: (String?) -> Unit = { _ -> exitFullScreenUseCaseCalled = true }
    private var exitFullScreenUseCaseCalled = false

    @Before
    fun setup() {
        exitFullScreenUseCaseCalled = false
        engine = mock()
        activity = mock()
    }

    @Test
    fun `feature registers itself on start`() {
        val feature = webAuthnFeature()

        feature.start()

        verify(engine).registerActivityDelegate(feature)
    }

    @Test
    fun `feature unregisters itself on stop`() {
        val feature = webAuthnFeature()

        feature.stop()

        verify(engine).unregisterActivityDelegate()
    }

    @Test
    fun `activity delegate starts intent sender`() {
        val feature = webAuthnFeature()
        val callback: ((Intent?) -> Unit) = { }
        val intentSender: IntentSender = mock()

        feature.startIntentSenderForResult(intentSender, callback)

        verify(activity).startIntentSenderForResult(eq(intentSender), anyInt(), nullable(), eq(0), eq(0), eq(0))
    }

    @Test
    fun `callback is invoked`() {
        val feature = webAuthnFeature()
        var callbackInvoked = false
        val callback: ((Intent?) -> Unit) = { callbackInvoked = true }
        val intentSender: IntentSender = mock()

        feature.onActivityResult(ACTIVITY_REQUEST_CODE, Intent(), 0)

        assertFalse(callbackInvoked)

        feature.startIntentSenderForResult(intentSender, callback)
        feature.onActivityResult(ACTIVITY_REQUEST_CODE + 1, Intent(), 0)

        assertTrue(callbackInvoked)
    }

    @Test
    fun `feature won't process results with the wrong request code`() {
        val feature = webAuthnFeature()

        val result = feature.onActivityResult(ACTIVITY_REQUEST_CODE - 5, Intent(), 0)

        assertFalse(result)
    }

    @Test
    fun `when feature called by activity, then exit fullscreen if required`() {
        val feature = webAuthnFeature()
        val callback: ((Intent?) -> Unit) = { }
        val intentSender: IntentSender = mock()

        assertFalse(exitFullScreenUseCaseCalled)

        feature.startIntentSenderForResult(intentSender, callback)

        assertTrue(exitFullScreenUseCaseCalled)
    }

    private fun webAuthnFeature(): WebAuthnFeature {
        return WebAuthnFeature(engine, activity, { exitFullScreen("") }) { "" }
    }
}
