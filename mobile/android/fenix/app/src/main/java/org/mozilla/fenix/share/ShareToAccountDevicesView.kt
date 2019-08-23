/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import android.view.LayoutInflater
import android.view.ViewGroup
import kotlinx.android.extensions.LayoutContainer
import mozilla.components.concept.sync.Device
import org.mozilla.fenix.R

/**
 * Callbacks for possible user interactions on the [ShareToAccountDevicesView]
 */
interface ShareToAccountDevicesInteractor {
    fun onSignIn()
    fun onAddNewDevice()
    fun onShareToDevice(device: Device)
    fun onShareToAllDevices(devices: List<Device>)
}

class ShareToAccountDevicesView(
    override val containerView: ViewGroup,
    private val interactor: ShareToAccountDevicesInteractor
) : LayoutContainer {
    init {
        LayoutInflater.from(containerView.context)
            .inflate(R.layout.share_to_account_devices, containerView, true)
    }
}
