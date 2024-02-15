/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import mozilla.components.service.nimbus.messaging.FxNimbusMessaging
import mozilla.components.service.nimbus.messaging.Messaging
import org.junit.Assert
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.experiments.nimbus.NimbusInterface
import org.mozilla.experiments.nimbus.internal.NimbusException
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.helpers.TestSetup
import org.mozilla.fenix.messaging.CustomAttributeProvider

/**
 * Test to instantiate Nimbus and automatically test all trigger expressions shipping with the app.
 *
 * We do this as a UI test to make sure that:
 * - as much of the custom targeting and trigger attributes are recorded as possible.
 * - we can run the Rust JEXL evaluator.
 */
class NimbusMessagingTriggerTest : TestSetup() {
    private lateinit var feature: Messaging
    private lateinit var nimbus: NimbusInterface

    @get:Rule
    val activityTestRule = HomeActivityIntentTestRule.withDefaultSettingsOverrides(skipOnboarding = true)

    @Before
    override fun setUp() {
        super.setUp()
        nimbus = TestHelper.appContext.components.nimbus.sdk
        feature = FxNimbusMessaging.features.messaging.value()
    }

    @Test
    fun testAllMessageTriggersAreValid() {
        val triggers = feature.triggers
        val customAttributes = CustomAttributeProvider.getCustomAttributes(TestHelper.appContext)
        val jexl = nimbus.createMessageHelper(customAttributes)

        val failed = mutableMapOf<String, String>()
        triggers.forEach { (key, expr) ->
            try {
                jexl.evalJexl(expr)
            } catch (e: NimbusException) {
                failed.put(key, expr)
            }
        }
        if (failed.isNotEmpty()) {
            Assert.fail("Expressions failed: $failed")
        }
    }

    @Test
    fun testBadTriggersAreDetected() {
        val jexl = nimbus.createMessageHelper()

        val triggers = mapOf(
            "Syntax error" to "|'syntax error'|",
            "Invalid identifier" to "invalid_identifier",
            "Invalid transform" to "'string'|invalid_transform",
            "Invalid interval" to "'string'|eventLastSeen('Invalid')",
        )

        triggers.forEach { (key, expr) ->
            try {
                jexl.evalJexl(expr)
                Assert.fail("$key expression failed to error: $expr")
            } catch (e: NimbusException) {
                // NOOP
            }
        }
    }
}
