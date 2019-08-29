/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import android.content.Intent
import androidx.fragment.app.Fragment
import androidx.navigation.NavController
import assertk.assertAll
import assertk.assertThat
import assertk.assertions.isEqualTo
import assertk.assertions.isTrue
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.slot
import io.mockk.spyk
import io.mockk.verify
import io.mockk.verifyOrder
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceEventOutgoing
import mozilla.components.concept.sync.DeviceType
import mozilla.components.concept.sync.OAuthAccount
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.TestApplication
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.share.listadapters.AppShareOption
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@UseExperimental(ObsoleteCoroutinesApi::class)
@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)
class ShareControllerTest {
    private val fragment = mockk<Fragment>(relaxed = true)
    private val tabsToShare = listOf(
        ShareTab("url0", "title0", "sessionId0"),
        ShareTab("url1", "title1")
    )
    private val textToShare = "${tabsToShare[0].url}\n${tabsToShare[1].url}"
    private val account = mockk<OAuthAccount>(relaxed = true)
    private val navController = mockk<NavController>(relaxed = true)
    private val dismiss = mockk<() -> Unit>(relaxed = true)
    // Use a spy that allows overriding "controller.sendTab below"
    private val controller = spyk(DefaultShareController(fragment, tabsToShare, account, navController, dismiss))

    @Test
    fun `handleShareClosed should call a passed in delegate to close this`() {
        controller.handleShareClosed()

        verify { dismiss() }
    }

    @Test
    fun `handleShareToApp should start a new sharing activity and close this`() {
        val appPackageName = "package"
        val appClassName = "activity"
        val appShareOption = AppShareOption("app", mockk(), appPackageName, appClassName)
        val shareIntent = slot<Intent>()
        every { fragment.startActivity(capture(shareIntent)) } just Runs

        controller.handleShareToApp(appShareOption)

        // Check that the Intent used for querying apps has the expected structre
        assertAll {
            assertThat(shareIntent.isCaptured).isTrue()
            assertThat(shareIntent.captured.action).isEqualTo(Intent.ACTION_SEND)
            assertThat(shareIntent.captured.extras!![Intent.EXTRA_TEXT]).isEqualTo(textToShare)
            assertThat(shareIntent.captured.type).isEqualTo("text/plain")
            assertThat(shareIntent.captured.flags).isEqualTo(Intent.FLAG_ACTIVITY_NEW_TASK)
            assertThat(shareIntent.captured.component!!.packageName).isEqualTo(appPackageName)
            assertThat(shareIntent.captured.component!!.className).isEqualTo(appClassName)
        }
        verifyOrder {
            fragment.startActivity(shareIntent.captured)
            dismiss()
        }
    }

    @Test
    @Suppress("DeferredResultUnused")
    fun `handleShareToDevice should share to account device, inform callbacks and dismiss`() {
        val deviceToShareTo =
            Device("deviceId", "deviceName", DeviceType.UNKNOWN, false, 0L, emptyList(), false, null)
        val tabSharedCallbackActivity = mockk<HomeActivity>(relaxed = true)
        val sharedTabsNumber = slot<Int>()
        val deviceId = slot<String>()
        every { fragment.activity } returns tabSharedCallbackActivity

        controller.handleShareToDevice(deviceToShareTo)

        // Verify all the needed methods are called.
        verify { controller.sendTab(capture(deviceId)) }
        verify { tabSharedCallbackActivity.onTabsShared(capture(sharedTabsNumber)) }
        verify { dismiss() }
        assertAll {
            // sendTab() should be called for each device in the account
            assertThat(deviceId.isCaptured).isTrue()
            assertThat(deviceId.captured).isEqualTo(deviceToShareTo.id)

            // All current tabs should be shared
            assertThat(sharedTabsNumber.isCaptured).isTrue()
            assertThat(sharedTabsNumber.captured).isEqualTo(tabsToShare.size)
        }
    }

    @Test
    fun `handleShareToAllDevices calls handleShareToDevice multiple times`() {
        val devicesToShareTo = listOf(
            Device("deviceId0", "deviceName0", DeviceType.UNKNOWN, false, 0L, emptyList(), false, null),
            Device("deviceId1", "deviceName1", DeviceType.UNKNOWN, true, 1L, emptyList(), false, null)
        )
        val tabSharedCallbackActivity = mockk<HomeActivity>(relaxed = true)
        val sharedTabsNumber = slot<Int>()
        val sharedToDeviceIds = mutableListOf<String>()
        every { fragment.activity } returns tabSharedCallbackActivity

        controller.handleShareToAllDevices(devicesToShareTo)

        // Verify all the needed methods are called. sendTab() should be called for each account device.
        verify(exactly = devicesToShareTo.size) { controller.sendTab(capture(sharedToDeviceIds)) }
        verify { tabSharedCallbackActivity.onTabsShared(capture(sharedTabsNumber)) }
        verify { dismiss() }
        assertAll {
            // sendTab() should be called for each device in the account
            assertThat(sharedToDeviceIds.size).isEqualTo(devicesToShareTo.size)
            sharedToDeviceIds.forEachIndexed { index, shareToDeviceId ->
                assertThat(shareToDeviceId).isEqualTo(devicesToShareTo[index].id)
            }

            // All current tabs should be shared
            assertThat(sharedTabsNumber.isCaptured).isTrue()
            assertThat(sharedTabsNumber.captured).isEqualTo(tabsToShare.size)
        }
    }

    @Test
    fun `handleSignIn should navigate to the Sync Fragment and dismiss this one`() {
        controller.handleSignIn()

        verifyOrder {
            navController.nav(
                R.id.shareFragment,
                ShareFragmentDirections.actionShareFragmentToTurnOnSyncFragment()
            )
            dismiss()
        }
    }

    @Test
    @Suppress("DeferredResultUnused")
    fun `sendTab should send all current tabs to the selected device`() {
        val deviceToShareTo =
            Device("deviceId", "deviceName", DeviceType.UNKNOWN, false, 0L, emptyList(), false, null)
        val sharedToDeviceIds = mutableListOf<String>()
        val outgoingEvents = mutableListOf<DeviceEventOutgoing.SendTab>()

        controller.sendTab(deviceToShareTo.id)

        // Verify the sync components being called and record the sent values
        verify {
            account.deviceConstellation()
                .sendEventToDeviceAsync(capture(sharedToDeviceIds), capture(outgoingEvents))
        }
        assertAll {
            // All Tabs should be sent to the same device
            assertThat(sharedToDeviceIds.size).isEqualTo(tabsToShare.size)
            sharedToDeviceIds.forEach { sharedToDeviceId ->
                assertThat(sharedToDeviceId).isEqualTo(deviceToShareTo.id)
            }
            // There should be an DeviceEventOutgoing.SendTab for each sent Tab
            assertThat(outgoingEvents.size).isEqualTo(outgoingEvents.size)
            outgoingEvents.forEachIndexed { index, event ->
                assertThat((event).title).isEqualTo(tabsToShare[index].title)
                assertThat((event).url).isEqualTo(tabsToShare[index].url)
            }
        }
    }

    @Test
    fun `getShareText should respect concatenate shared tabs urls`() {
        assertThat(controller.getShareText()).isEqualTo(textToShare)
    }
}
