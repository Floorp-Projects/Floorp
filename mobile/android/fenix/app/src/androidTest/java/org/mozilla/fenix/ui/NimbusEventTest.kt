/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import android.content.Intent
import io.mockk.mockk
import mozilla.components.concept.sync.AuthType
import mozilla.components.service.fxa.FirefoxAccount
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.components.TelemetryAccountObserver
import org.mozilla.fenix.helpers.Experimentation
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.RetryTestRule
import org.mozilla.fenix.helpers.TestHelper.appContext
import org.mozilla.fenix.helpers.TestSetup

class NimbusEventTest : TestSetup() {
    @get:Rule
    val homeActivityTestRule = HomeActivityIntentTestRule.withDefaultSettingsOverrides()
        .withIntent(
            Intent().apply {
                action = Intent.ACTION_VIEW
            },
        )

    @Rule
    @JvmField
    val retryTestRule = RetryTestRule(3)

    @Test
    fun homeScreenNimbusEventsTest() {
        Experimentation.withHelper {
            assertTrue(evalJexl("'app_opened'|eventSum('Days', 28, 0) > 0"))
        }
    }

    @Test
    fun telemetryAccountObserverTest() {
        val observer = TelemetryAccountObserver(appContext)
        // replacing interface mock with implementation mock.
        observer.onAuthenticated(mockk<FirefoxAccount>(), AuthType.Signin)

        Experimentation.withHelper {
            assertTrue(evalJexl("'sync_auth.sign_in'|eventSum('Days', 28, 0) > 0"))
        }
    }
}
