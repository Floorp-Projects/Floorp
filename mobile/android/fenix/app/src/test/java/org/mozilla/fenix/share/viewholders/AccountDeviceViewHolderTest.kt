/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share.viewholders

import android.view.LayoutInflater
import io.mockk.Called
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceType
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.databinding.AccountShareListItemBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.share.ShareToAccountDevicesInteractor
import org.mozilla.fenix.share.listadapters.SyncShareOption

@RunWith(FenixRobolectricTestRunner::class)
class AccountDeviceViewHolderTest {

    private val baseDevice = Device(
        id = "",
        displayName = "",
        deviceType = DeviceType.UNKNOWN,
        isCurrentDevice = true,
        lastAccessTime = 0L,
        capabilities = emptyList(),
        subscriptionExpired = false,
        subscription = null
    )
    private lateinit var binding: AccountShareListItemBinding
    private lateinit var viewHolder: AccountDeviceViewHolder
    private lateinit var interactor: ShareToAccountDevicesInteractor

    @Before
    fun setup() {
        interactor = mockk(relaxUnitFun = true)

        binding = AccountShareListItemBinding.inflate(LayoutInflater.from(testContext))
        viewHolder = AccountDeviceViewHolder(binding.root, interactor)
    }

    @Test
    fun `bind SignIn option`() {
        viewHolder.bind(SyncShareOption.SignIn)
        assertEquals("Sign in to Sync", binding.deviceName.text)

        viewHolder.itemView.performClick()
        verify { interactor.onSignIn() }
        assertFalse(viewHolder.itemView.hasOnClickListeners())
    }

    @Test
    fun `bind Reconnect option`() {
        viewHolder.bind(SyncShareOption.Reconnect)
        assertEquals("Reconnect to Sync", binding.deviceName.text)

        viewHolder.itemView.performClick()
        verify { interactor.onReauth() }
        assertFalse(viewHolder.itemView.hasOnClickListeners())
    }

    @Test
    fun `bind Offline option`() {
        viewHolder.bind(SyncShareOption.Offline)
        assertEquals("Offline", binding.deviceName.text)

        viewHolder.itemView.performClick()
        verify { interactor wasNot Called }
        assertFalse(viewHolder.itemView.hasOnClickListeners())
    }

    @Test
    fun `bind AddNewDevice option`() {
        viewHolder.bind(SyncShareOption.AddNewDevice)
        assertEquals("Connect another device", binding.deviceName.text)

        viewHolder.itemView.performClick()
        verify { interactor.onAddNewDevice() }
        assertFalse(viewHolder.itemView.hasOnClickListeners())
    }

    @Test
    fun `bind SendAll option`() {
        val devices = listOf<Device>(mockk())
        viewHolder.bind(SyncShareOption.SendAll(devices))
        assertEquals("Send to all devices", binding.deviceName.text)

        viewHolder.itemView.performClick()
        verify { interactor.onShareToAllDevices(devices) }
        assertFalse(viewHolder.itemView.hasOnClickListeners())
    }

    @Test
    fun `bind mobile SingleDevice option`() {
        val device = baseDevice.copy(
            deviceType = DeviceType.MOBILE,
            displayName = "Mobile"
        )
        viewHolder.bind(SyncShareOption.SingleDevice(device))
        assertEquals("Mobile", binding.deviceName.text)

        viewHolder.itemView.performClick()
        verify { interactor.onShareToDevice(device) }
        assertFalse(viewHolder.itemView.hasOnClickListeners())
    }

    @Test
    fun `bind desktop SingleDevice option`() {
        val device = baseDevice.copy(
            deviceType = DeviceType.DESKTOP,
            displayName = "Desktop"
        )
        viewHolder.bind(SyncShareOption.SingleDevice(device))
        assertEquals("Desktop", binding.deviceName.text)

        viewHolder.itemView.performClick()
        verify { interactor.onShareToDevice(device) }
        assertFalse(viewHolder.itemView.hasOnClickListeners())
    }
}
