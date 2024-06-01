/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs.commands

import mozilla.components.browser.storage.sync.RemoteTabsCommandQueue
import mozilla.components.concept.sync.ConstellationState
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.DeviceCommandOutgoing
import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.DeviceType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

class CloseTabsCommandSenderTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private val device123 = Device(
        id = "123",
        displayName = "Charcoal",
        deviceType = DeviceType.DESKTOP,
        isCurrentDevice = false,
        lastAccessTime = null,
        capabilities = listOf(DeviceCapability.CLOSE_TABS),
        subscriptionExpired = true,
        subscription = null,
    )

    private val device1234 = Device(
        id = "1234",
        displayName = "Ruby",
        deviceType = DeviceType.DESKTOP,
        isCurrentDevice = false,
        lastAccessTime = null,
        capabilities = emptyList(),
        subscriptionExpired = true,
        subscription = null,
    )

    private val accountManager: FxaAccountManager = mock()
    private val account: OAuthAccount = mock()
    private val constellation: DeviceConstellation = mock()
    private val state: ConstellationState = mock()
    private val sender = CloseTabsCommandSender(accountManager)

    @Before
    fun setUp() {
        whenever(accountManager.authenticatedAccount()).thenReturn(account)
        whenever(account.deviceConstellation()).thenReturn(constellation)
        whenever(constellation.state()).thenReturn(state)
    }

    @Test
    fun `GIVEN a device with the close tabs capability WHEN sending the command to the device succeeds THEN the result is a success`() = runTestOnMain {
        whenever(state.otherDevices).thenReturn(listOf(device123))
        whenever(constellation.sendCommandToDevice(eq("123"), any())).thenReturn(true)

        assertEquals(RemoteTabsCommandQueue.SendResult.Ok, sender.send("123", DeviceCommandOutgoing.CloseTab(urls = listOf("http://example.com"))))
    }

    @Test
    fun `GIVEN a device with the close tabs capability WHEN sending the command to the device fails THEN the result is a failure`() = runTestOnMain {
        whenever(state.otherDevices).thenReturn(listOf(device123))
        whenever(constellation.sendCommandToDevice(eq("123"), any())).thenReturn(false)

        assertEquals(RemoteTabsCommandQueue.SendResult.Error, sender.send("123", DeviceCommandOutgoing.CloseTab(urls = listOf("http://example.com"))))
    }

    @Test
    fun `GIVEN a device without the close tabs capability WHEN sending the command to the device THEN the result is a failure`() = runTestOnMain {
        whenever(state.otherDevices).thenReturn(listOf(device1234))

        assertEquals(RemoteTabsCommandQueue.SendResult.NoDevice, sender.send("1234", DeviceCommandOutgoing.CloseTab(urls = listOf("http://example.com"))))

        verify(constellation, never()).sendCommandToDevice(any(), any())
    }

    @Test
    fun `GIVEN the user is not signed in WHEN sending the command to the device THEN the result is a failure`() = runTestOnMain {
        whenever(accountManager.authenticatedAccount()).thenReturn(null)

        assertEquals(RemoteTabsCommandQueue.SendResult.NoAccount, sender.send("123", DeviceCommandOutgoing.CloseTab(urls = listOf("http://example.com"))))

        verify(account, never()).deviceConstellation()
        verify(constellation, never()).sendCommandToDevice(any(), any())
    }
}
