/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts.push

import mozilla.components.concept.sync.ConstellationState
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.DeviceType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`

class CloseTabsUseCasesTest {
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

    private val manager: FxaAccountManager = mock()
    private val account: OAuthAccount = mock()
    private val constellation: DeviceConstellation = mock()
    private val state: ConstellationState = mock()

    @Before
    fun setUp() {
        `when`(manager.authenticatedAccount()).thenReturn(account)
        `when`(account.deviceConstellation()).thenReturn(constellation)
        `when`(constellation.state()).thenReturn(state)
    }

    @Test
    fun `GIVEN a list of devices WHEN one device supports the close tabs command THEN filtering returns that device`() {
        val deviceIds = mutableListOf<String>()
        `when`(state.otherDevices).thenReturn(listOf(device123, device1234))
        filterCloseTabsDevices(manager) { _, devices ->
            deviceIds.addAll(devices.map { it.id })
        }

        assertEquals(listOf("123"), deviceIds)
    }

    @Test
    fun `GIVEN a constellation with one capable device WHEN sending a close tabs command to that device THEN the command is sent`() = runTestOnMain {
        val useCases = CloseTabsUseCases(manager)

        `when`(state.otherDevices).thenReturn(listOf(device123))
        `when`(constellation.sendCommandToDevice(any(), any()))
            .thenReturn(true)

        useCases.close("123", "http://example.com")

        verify(constellation).sendCommandToDevice(any(), any())
    }

    @Test
    fun `GIVEN a constellation with one incapable device WHEN sending a close tabs command to that device THEN the command is not sent`() = runTestOnMain {
        val useCases = CloseTabsUseCases(manager)

        `when`(state.otherDevices).thenReturn(listOf(device1234))
        `when`(constellation.sendCommandToDevice(any(), any()))
            .thenReturn(false)

        useCases.close("1234", "http://example.com")

        verify(constellation, never()).sendCommandToDevice(any(), any())
    }
}
