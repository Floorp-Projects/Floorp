/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import android.content.Intent
import androidx.fragment.app.Fragment
import androidx.navigation.NavController
import assertk.assertAll
import assertk.assertThat
import assertk.assertions.isDataClassEqualTo
import assertk.assertions.isEqualTo
import assertk.assertions.isTrue
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.slot
import io.mockk.verify
import io.mockk.verifyOrder
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceType
import mozilla.components.concept.sync.TabData
import mozilla.components.feature.sendtab.SendTabUseCases
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
    private val shareTabs = listOf(
        ShareTab("url0", "title0"),
        ShareTab("url1", "title1")
    )
    // Navigation between app fragments uses ShareTab as arguments. SendTabUseCases uses TabData.
    private val tabsData = listOf(
        TabData("title0", "url0"),
        TabData("title1", "url1")
    )
    private val textToShare = "${shareTabs[0].url}\n${shareTabs[1].url}"
    private val sendTabUseCases = mockk<SendTabUseCases>(relaxed = true)
    private val navController = mockk<NavController>(relaxed = true)
    private val dismiss = mockk<() -> Unit>(relaxed = true)
    private val controller = DefaultShareController(fragment, shareTabs, sendTabUseCases, navController, dismiss)

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
        val tabsShared = slot<List<TabData>>()
        every { fragment.activity } returns tabSharedCallbackActivity

        controller.handleShareToDevice(deviceToShareTo)

        // Verify all the needed methods are called.
        verifyOrder {
            sendTabUseCases.sendToDeviceAsync(capture(deviceId), capture(tabsShared))
            tabSharedCallbackActivity.onTabsShared(capture(sharedTabsNumber))
            dismiss()
        }
        assertAll {
            assertThat(deviceId.isCaptured).isTrue()
            assertThat(deviceId.captured).isEqualTo(deviceToShareTo.id)
            assertThat(tabsShared.isCaptured).isTrue()
            assertThat(tabsShared.captured).isEqualTo(tabsData)

            // All current tabs should be shared
            assertThat(sharedTabsNumber.isCaptured).isTrue()
            assertThat(sharedTabsNumber.captured).isEqualTo(shareTabs.size)
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
        val tabsShared = slot<List<TabData>>()
        every { fragment.activity } returns tabSharedCallbackActivity

        controller.handleShareToAllDevices(devicesToShareTo)

        // Verify all the needed methods are called. sendTab() should be called for each account device.
        verifyOrder {
            sendTabUseCases.sendToAllAsync(capture(tabsShared))
            tabSharedCallbackActivity.onTabsShared(capture(sharedTabsNumber))
            dismiss()
        }
        assertAll {
            // SendTabUseCases should send a the `shareTabs` mapped to tabData
            assertThat(tabsShared.isCaptured).isTrue()
            assertThat(tabsShared.captured).isEqualTo(tabsData)

            // All current tabs should be shared
            assertThat(sharedTabsNumber.isCaptured).isTrue()
            assertThat(sharedTabsNumber.captured).isEqualTo(shareTabs.size)
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
    fun `getShareText should respect concatenate shared tabs urls`() {
        assertThat(controller.getShareText()).isEqualTo(textToShare)
    }

    @Test
    fun `ShareTab#toTabData maps a ShareTab to a TabData`() {
        var tabData: TabData

        with(controller) {
            tabData = shareTabs[0].toTabData()
        }

        assertThat(tabData).isDataClassEqualTo(tabsData[0])
    }

    @Test
    fun `ShareTab#toTabData maps a list of ShareTab to a TabData list`() {
        var tabData: List<TabData>

        with(controller) {
            tabData = shareTabs.toTabData()
        }

        assertThat(tabData).isEqualTo(tabsData)
    }
}
