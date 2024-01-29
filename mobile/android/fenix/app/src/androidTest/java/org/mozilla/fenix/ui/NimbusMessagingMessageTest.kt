/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import android.content.Context
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import kotlinx.coroutines.test.runTest
import mozilla.components.service.nimbus.messaging.FxNimbusMessaging
import mozilla.components.service.nimbus.messaging.Messaging
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.TestHelper

/**
 * This test is to test the integrity of messages hardcoded in the FML.
 *
 * It tests if the trigger expressions are valid, all the fields are complete
 * and a simple check if they are localized (don't contain `_`).
 */
class NimbusMessagingMessageTest {
    private lateinit var feature: Messaging
    private lateinit var mDevice: UiDevice

    private lateinit var context: Context

    private val messaging
        get() = context.components.nimbus.messaging

    @get:Rule
    val activityTestRule =
        HomeActivityIntentTestRule.withDefaultSettingsOverrides(skipOnboarding = true)

    @Before
    fun setUp() {
        context = TestHelper.appContext
        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        feature = FxNimbusMessaging.features.messaging.value()
    }

    /**
     * Check if all messages in the FML are internally consistent with the
     * rest of the FML. This check is done in the `NimbusMessagingStorage`
     * class.
     */
    @Test
    fun testAllMessageIntegrity() = runTest {
        val messages = messaging.getMessages()
        val rawMessages = feature.messages
        assertTrue(rawMessages.isNotEmpty())

        if (messages.size != rawMessages.size) {
            val expected = rawMessages.keys.toHashSet()
            val observed = messages.map { it.id }.toHashSet()
            val missing = expected - observed
            fail("Problem with message(s) in FML: $missing")
        }
        assertEquals(messages.size, rawMessages.size)
    }

    private fun checkIsLocalized(string: String) {
        assertFalse(string.isBlank())
        // The check will almost always succeed, since the generated code
        // will not compile if this is true, and there is no resource available.
        assertFalse(string.matches(Regex("[a-z][_a-z\\d]*")))
    }

    /**
     * Check that the messages are localized.
     */
    @Test
    fun testAllMessagesAreLocalized() {
        feature.messages.values.forEach { message ->
            message.buttonLabel?.let(::checkIsLocalized)
            message.title?.let(::checkIsLocalized)
            checkIsLocalized(message.text)
        }
    }

    @Test
    fun testIndividualMessagesAreValid() {
        val expectedMessages = listOf(
            "default-browser",
            "default-browser-notification",
        )
        val rawMessages = feature.messages
        for (id in expectedMessages) {
            assertTrue(rawMessages.containsKey(id))
        }
    }
}
