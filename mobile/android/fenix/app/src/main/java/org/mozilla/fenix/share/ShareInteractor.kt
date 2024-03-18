/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import mozilla.components.concept.sync.Device
import org.mozilla.fenix.share.listadapters.AppShareOption

/**
 * Interactor for the share screen.
 */
class ShareInteractor(
    private val controller: ShareController,
) : ShareCloseInteractor,
    ShareToAccountDevicesInteractor,
    ShareToAppsInteractor,
    SaveToPDFInteractor {
    override fun onReauth() {
        controller.handleReauth()
    }

    override fun onShareClosed() {
        controller.handleShareClosed()
    }

    override fun onSignIn() {
        controller.handleSignIn()
    }

    override fun onAddNewDevice() {
        controller.handleAddNewDevice()
    }

    override fun onShareToDevice(device: Device) {
        controller.handleShareToDevice(device)
    }

    override fun onShareToAllDevices(devices: List<Device>) {
        controller.handleShareToAllDevices(devices)
    }

    override fun onShareToApp(appToShareTo: AppShareOption) {
        controller.handleShareToApp(appToShareTo)
    }

    override fun onSaveToPDF(tabId: String?) {
        controller.handleSaveToPDF(tabId)
    }

    override fun onPrint(tabId: String?) {
        controller.handlePrint(tabId)
    }
}
