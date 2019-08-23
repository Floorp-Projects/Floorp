/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import mozilla.components.concept.sync.Device
import org.mozilla.fenix.share.listadapters.Application

/**
 * Interactor for the share screen.
 */
class ShareInteractor : ShareCloseInteractor, ShareToAccountDevicesInteractor, ShareToAppsInteractor {
    override fun onShareClosed() {
        TODO("not yet!? implemented")
    }

    override fun onSignIn() {
        TODO("not yet!? implemented")
    }

    override fun onAddNewDevice() {
        TODO("not yet!? implemented")
    }

    override fun onShareToDevice(device: Device) {
        TODO("not yet!? implemented")
    }

    override fun onShareToAllDevices(devices: List<Device>) {
        TODO("not yet!? implemented")
    }

    override fun onShareToApp(appToShareTo: Application) {
        TODO("not yet!? implemented")
    }
}
