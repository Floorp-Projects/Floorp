/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wifi

import io.mockk.Called
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.feature.sitepermissions.SitePermissionsRules.Action
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.settings.PhoneFeature.AUTOPLAY_AUDIBLE
import org.mozilla.fenix.settings.PhoneFeature.AUTOPLAY_INAUDIBLE
import org.mozilla.fenix.settings.sitepermissions.AUTOPLAY_ALLOW_ALL
import org.mozilla.fenix.settings.sitepermissions.AUTOPLAY_ALLOW_ON_WIFI
import org.mozilla.fenix.settings.sitepermissions.AUTOPLAY_BLOCK_ALL
import org.mozilla.fenix.utils.Settings

class SitePermissionsWifiIntegrationTest {

    private lateinit var settings: Settings
    private lateinit var wifiConnectionMonitor: WifiConnectionMonitor
    private lateinit var wifiIntegration: SitePermissionsWifiIntegration

    @Before
    fun setup() {
        settings = mockk()
        wifiConnectionMonitor = mockk(relaxed = true)
        wifiIntegration = SitePermissionsWifiIntegration(settings, wifiConnectionMonitor)

        every { settings.setSitePermissionsPhoneFeatureAction(any(), any()) } just Runs
    }

    @Test
    fun `add and remove wifi connected listener`() {
        wifiIntegration.addWifiConnectedListener()
        verify { wifiConnectionMonitor.register(any()) }

        wifiIntegration.removeWifiConnectedListener()
        verify { wifiConnectionMonitor.unregister(any()) }
    }

    @Test
    fun `start and stop wifi connection monitor`() {
        wifiIntegration.start()
        verify { wifiConnectionMonitor.start() }

        wifiIntegration.stop()
        verify { wifiConnectionMonitor.stop() }
    }

    @Test
    fun `add only if autoplay is only allowed on wifi`() {
        every { settings.getAutoplayUserSetting(default = AUTOPLAY_BLOCK_ALL) } returns AUTOPLAY_ALLOW_ALL
        wifiIntegration.maybeAddWifiConnectedListener()
        verify { wifiConnectionMonitor wasNot Called }

        every { settings.getAutoplayUserSetting(default = AUTOPLAY_BLOCK_ALL) } returns AUTOPLAY_ALLOW_ON_WIFI
        wifiIntegration.maybeAddWifiConnectedListener()
        verify { wifiConnectionMonitor.register(any()) }
    }

    @Test
    fun `listener removes itself if autoplay is not only allowed on wifi`() {
        every { settings.getAutoplayUserSetting(default = AUTOPLAY_BLOCK_ALL) } returns AUTOPLAY_ALLOW_ALL
        wifiIntegration.onWifiConnectionChanged(connected = true)
        verify { wifiConnectionMonitor.unregister(any()) }
    }

    @Test
    fun `listener sets audible and inaudible settings to allowed on connect`() {
        every { settings.getAutoplayUserSetting(default = AUTOPLAY_BLOCK_ALL) } returns AUTOPLAY_ALLOW_ON_WIFI
        wifiIntegration.onWifiConnectionChanged(connected = true)
        verify { settings.setSitePermissionsPhoneFeatureAction(AUTOPLAY_AUDIBLE, Action.ALLOWED) }
        verify { settings.setSitePermissionsPhoneFeatureAction(AUTOPLAY_INAUDIBLE, Action.ALLOWED) }
    }

    @Test
    fun `listener sets audible and inaudible settings to blocked on disconnected`() {
        every { settings.getAutoplayUserSetting(default = AUTOPLAY_BLOCK_ALL) } returns AUTOPLAY_ALLOW_ON_WIFI
        wifiIntegration.onWifiConnectionChanged(connected = false)
        verify { settings.setSitePermissionsPhoneFeatureAction(AUTOPLAY_AUDIBLE, Action.BLOCKED) }
        verify { settings.setSitePermissionsPhoneFeatureAction(AUTOPLAY_INAUDIBLE, Action.BLOCKED) }
    }
}
