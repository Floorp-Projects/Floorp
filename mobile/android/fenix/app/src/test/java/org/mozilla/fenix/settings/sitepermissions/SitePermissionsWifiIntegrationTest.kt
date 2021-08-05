/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.sitepermissions

import io.mockk.every
import io.mockk.verify
import io.mockk.mockk
import mozilla.components.feature.sitepermissions.SitePermissionsRules.Action.ALLOWED
import mozilla.components.feature.sitepermissions.SitePermissionsRules.Action.BLOCKED
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.settings.PhoneFeature.AUTOPLAY_AUDIBLE
import org.mozilla.fenix.settings.PhoneFeature.AUTOPLAY_INAUDIBLE
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.wifi.SitePermissionsWifiIntegration
import org.mozilla.fenix.wifi.WifiConnectionMonitor

class SitePermissionsWifiIntegrationTest {
    lateinit var settings: Settings
    lateinit var wifiIntegration: SitePermissionsWifiIntegration
    lateinit var wifiConnectionMonitor: WifiConnectionMonitor

    @Before
    fun setUp() {
        settings = mockk(relaxed = true)
        wifiConnectionMonitor = mockk(relaxed = true)
        wifiIntegration = SitePermissionsWifiIntegration(settings, wifiConnectionMonitor)
    }

    @Test
    fun `GIVEN auto play is set to be on allow only on wifi WHEN the feature starts THEN listen for wifi changes`() {
        every { settings.getAutoplayUserSetting() } returns AUTOPLAY_ALLOW_ON_WIFI

        wifiIntegration.start()

        verify(exactly = 1) {
            wifiConnectionMonitor.start()
        }
        verify(exactly = 1) {
            wifiConnectionMonitor.addOnWifiConnectedChangedListener(wifiIntegration.wifiConnectedListener)
        }
    }

    @Test
    fun `GIVEN auto play is not set to allow only on wifi WHEN the feature starts THEN will not listen for wifi changes`() {
        val autoPlaySettings =
            listOf(AUTOPLAY_BLOCK_ALL, AUTOPLAY_BLOCK_AUDIBLE, AUTOPLAY_ALLOW_ALL)

        autoPlaySettings.forEach { autoPlaySetting ->
            every { settings.getAutoplayUserSetting() } returns autoPlaySetting

            wifiIntegration.start()

            verify(exactly = 0) {
                wifiConnectionMonitor.addOnWifiConnectedChangedListener(wifiIntegration.wifiConnectedListener)
            }
            verify(exactly = 0) {
                wifiConnectionMonitor.addOnWifiConnectedChangedListener(wifiIntegration.wifiConnectedListener)
            }
        }
    }

    @Test
    fun `WHEN stopping the feature THEN all listeners will be removed`() {
        wifiIntegration.stop()

        verify(exactly = 1) {
            wifiConnectionMonitor.stop()
        }
        verify(exactly = 1) {
            wifiConnectionMonitor.removeOnWifiConnectedChangedListener(wifiIntegration.wifiConnectedListener)
        }
    }

    @Test
    fun `GIVEN wifi is connected and autoplay is set to allow only on wifi WHEN wifi changes to connected THEN the autoplay setting must be allowed`() {
        every { settings.getAutoplayUserSetting() } returns AUTOPLAY_ALLOW_ON_WIFI

        wifiIntegration.wifiConnectedListener(true)

        verify(exactly = 1) {
            settings.setSitePermissionsPhoneFeatureAction(AUTOPLAY_AUDIBLE, ALLOWED)
        }
        verify(exactly = 1) {
            settings.setSitePermissionsPhoneFeatureAction(AUTOPLAY_INAUDIBLE, ALLOWED)
        }
    }

    @Test
    fun `GIVEN wifi is connected and autoplay is set to allow only on wifi WHEN wifi changes to not connected THEN the autoplay setting must be blocked`() {
        every { settings.getAutoplayUserSetting() } returns AUTOPLAY_ALLOW_ON_WIFI

        wifiIntegration.wifiConnectedListener(false)

        verify(exactly = 1) {
            settings.setSitePermissionsPhoneFeatureAction(AUTOPLAY_AUDIBLE, BLOCKED)
        }

        verify(exactly = 1) {
            settings.setSitePermissionsPhoneFeatureAction(AUTOPLAY_INAUDIBLE, BLOCKED)
        }
    }

    @Test
    fun `GIVEN wifi is connected and autoplay is different from allow on wifi WHEN wifi changes THEN all the wifi listener will be stopped`() {
        val autoPlaySettings = listOf(AUTOPLAY_BLOCK_ALL, AUTOPLAY_BLOCK_AUDIBLE, AUTOPLAY_ALLOW_ALL)

        autoPlaySettings.forEach { autoPlaySetting ->
            every { settings.getAutoplayUserSetting() } returns autoPlaySetting

            wifiIntegration.wifiConnectedListener(true)
            wifiIntegration.wifiConnectedListener(false)

            verify(atLeast = 1) {
                wifiConnectionMonitor.stop()
                wifiConnectionMonitor.removeOnWifiConnectedChangedListener(wifiIntegration.wifiConnectedListener)
            }
            verify(atLeast = 1) {
                wifiConnectionMonitor.removeOnWifiConnectedChangedListener(wifiIntegration.wifiConnectedListener)
            }

            verify(exactly = 0) {
                settings.setSitePermissionsPhoneFeatureAction(any(), any())
            }
        }
    }
}
