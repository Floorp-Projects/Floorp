/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customtabs

import io.mockk.mockk
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.CustomTabConfig
import mozilla.components.browser.state.state.ExternalAppType
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.robolectric.testContext
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class PoweredByNotificationTest {

    @Test
    fun `register receiver on resume`() {
        val config = CustomTabConfig(externalAppType = ExternalAppType.TRUSTED_WEB_ACTIVITY)
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(
                    createCustomTab("https://mozilla.org", config = config),
                ),
            ),
        )

        val feature = PoweredByNotification(testContext, store, "session-id", mockk())
        feature.onResume(mockk())
    }

    @Test
    fun `don't register receiver if not in a TWA`() {
        val config = CustomTabConfig(externalAppType = ExternalAppType.PROGRESSIVE_WEB_APP)
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(
                    createCustomTab("https://mozilla.org", config = config),
                ),
            ),
        )

        val feature = PoweredByNotification(testContext, store, "session-id", mockk())
        feature.onResume(mockk())
    }

    @Test
    fun `unregister receiver on pause`() {
        val feature = PoweredByNotification(testContext, mockk(), "session-id", mockk())
        feature.onPause(mockk())
    }
}
