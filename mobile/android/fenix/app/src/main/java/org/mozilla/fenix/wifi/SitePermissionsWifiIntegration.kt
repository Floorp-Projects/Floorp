/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wifi

import mozilla.components.feature.sitepermissions.SitePermissionsRules
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.settings.sitepermissions.AUTOPLAY_ALLOW_ON_WIFI
import org.mozilla.fenix.settings.sitepermissions.AUTOPLAY_BLOCK_ALL
import org.mozilla.fenix.utils.Settings

/**
 * Handles implementation details of only setting up a WIFI connectivity listener if the current
 * user settings require it.
 */
class SitePermissionsWifiIntegration(
    private val settings: Settings,
    private val wifiConnectionMonitor: WifiConnectionMonitor
) : LifecycleAwareFeature {

    /**
     * Adds listener for autoplay setting [AUTOPLAY_ALLOW_ON_WIFI]. Sets all autoplay to allowed when
     * WIFI is connected, blocked otherwise.
     */
    private val wifiConnectedListener: ((Boolean) -> Unit) by lazy {
        { connected: Boolean ->
            val setting =
                if (connected) SitePermissionsRules.Action.ALLOWED else SitePermissionsRules.Action.BLOCKED
            if (settings.getAutoplayUserSetting(default = AUTOPLAY_BLOCK_ALL) == AUTOPLAY_ALLOW_ON_WIFI) {
                settings.setSitePermissionsPhoneFeatureAction(
                    PhoneFeature.AUTOPLAY_AUDIBLE,
                    setting
                )
                settings.setSitePermissionsPhoneFeatureAction(
                    PhoneFeature.AUTOPLAY_INAUDIBLE,
                    setting
                )
            } else {
                // The autoplay setting has changed, we can remove the listener
                removeWifiConnectedListener()
            }
        }
    }

    /**
     * If autoplay is only enabled on WIFI, sets a WIFI listener to set them accordingly. Otherwise
     * noop.
     */
    fun maybeAddWifiConnectedListener() {
        if (settings.getAutoplayUserSetting(default = AUTOPLAY_BLOCK_ALL) == AUTOPLAY_ALLOW_ON_WIFI) {
            addWifiConnectedListener()
        }
    }

    fun addWifiConnectedListener() {
        wifiConnectionMonitor.addOnWifiConnectedChangedListener(wifiConnectedListener)
    }

    fun removeWifiConnectedListener() {
        wifiConnectionMonitor.removeOnWifiConnectedChangedListener(wifiConnectedListener)
    }

    // Until https://bugzilla.mozilla.org/show_bug.cgi?id=1621825 is fixed, AUTOPLAY_ALLOW_ALL
    // only works while WIFI is active, so we are not using AUTOPLAY_ALLOW_ON_WIFI (or this class).
    // Once that is fixed, [start] and [maybeAddWifiConnectedListener] will need to be called on
    // activity startup.
    override fun start() {
        wifiConnectionMonitor.start()
    }

    override fun stop() {
        wifiConnectionMonitor.stop()
    }
}
