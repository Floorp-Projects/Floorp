/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wifi

import androidx.annotation.VisibleForTesting
import mozilla.components.feature.sitepermissions.SitePermissionsRules.Action.ALLOWED
import mozilla.components.feature.sitepermissions.SitePermissionsRules.Action.BLOCKED
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.settings.sitepermissions.AUTOPLAY_ALLOW_ON_WIFI
import org.mozilla.fenix.utils.Settings

/**
 * Handles implementation details of only setting up a WIFI connectivity listener if the current
 * user settings require it.
 */
class SitePermissionsWifiIntegration(
    private val settings: Settings,
    private val wifiConnectionMonitor: WifiConnectionMonitor,
) : LifecycleAwareFeature {

    /**
     * Adds listener for autoplay setting [AUTOPLAY_ALLOW_ON_WIFI]. Sets all autoplay to allowed when
     * WIFI is connected, blocked otherwise.
     */
    @VisibleForTesting
    internal val wifiConnectedListener: ((Boolean) -> Unit) by lazy {
        { connected: Boolean ->
            if (settings.getAutoplayUserSetting() == AUTOPLAY_ALLOW_ON_WIFI) {
                val setting = if (connected) ALLOWED else BLOCKED
                settings.setSitePermissionsPhoneFeatureAction(
                    PhoneFeature.AUTOPLAY_AUDIBLE,
                    setting,
                )
                settings.setSitePermissionsPhoneFeatureAction(
                    PhoneFeature.AUTOPLAY_INAUDIBLE,
                    setting,
                )
            } else {
                // The autoplay setting has changed, we can remove the listener
                stop()
            }
        }
    }

    private fun addWifiConnectedListener() {
        wifiConnectionMonitor.addOnWifiConnectedChangedListener(wifiConnectedListener)
    }

    private fun removeWifiConnectedListener() {
        wifiConnectionMonitor.removeOnWifiConnectedChangedListener(wifiConnectedListener)
    }

    override fun start() {
        if (settings.getAutoplayUserSetting() == AUTOPLAY_ALLOW_ON_WIFI) {
            wifiConnectionMonitor.start()
            addWifiConnectedListener()
        }
    }

    override fun stop() {
        wifiConnectionMonitor.stop()
        removeWifiConnectedListener()
    }
}
