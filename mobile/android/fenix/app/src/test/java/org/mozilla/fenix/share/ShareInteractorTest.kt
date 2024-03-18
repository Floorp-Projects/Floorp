/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import io.mockk.mockk
import io.mockk.verify
import mozilla.components.concept.sync.Device
import org.junit.Test
import org.mozilla.fenix.share.listadapters.AppShareOption

class ShareInteractorTest {
    private val controller = mockk<ShareController>(relaxed = true)
    private val interactor = ShareInteractor(controller)

    @Test
    fun onShareClosed() {
        interactor.onShareClosed()

        verify { controller.handleShareClosed() }
    }

    @Test
    fun onSignIn() {
        interactor.onSignIn()

        verify { controller.handleSignIn() }
    }

    @Test
    fun onReauth() {
        interactor.onReauth()

        verify { controller.handleReauth() }
    }

    @Test
    fun onAddNewDevice() {
        interactor.onAddNewDevice()

        verify { controller.handleAddNewDevice() }
    }

    @Test
    fun onShareToDevice() {
        val device = mockk<Device>()

        interactor.onShareToDevice(device)

        verify { controller.handleShareToDevice(device) }
    }

    @Test
    fun onSendToAllDevices() {
        val devices = emptyList<Device>()

        interactor.onShareToAllDevices(devices)

        verify { controller.handleShareToAllDevices(devices) }
    }

    @Test
    fun onShareToApp() {
        val app = mockk<AppShareOption>()

        interactor.onShareToApp(app)

        verify { controller.handleShareToApp(app) }
    }

    @Test
    fun `WHEN onSaveToPDF is call THEN call handleSaveToPDF`() {
        interactor.onSaveToPDF("tabID")

        verify { controller.handleSaveToPDF("tabID") }
    }

    @Test
    fun `WHEN onPrint is call THEN call handlePrint`() {
        interactor.onPrint("tabID")
        verify { controller.handlePrint("tabID") }
    }
}
