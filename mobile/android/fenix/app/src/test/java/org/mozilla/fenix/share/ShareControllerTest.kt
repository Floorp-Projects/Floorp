/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import android.app.Activity
import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import androidx.navigation.NavController
import com.google.android.material.snackbar.Snackbar
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.slot
import io.mockk.spyk
import io.mockk.verify
import io.mockk.verifyOrder
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.TestCoroutineScope
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceType
import mozilla.components.concept.sync.TabData
import mozilla.components.feature.accounts.push.SendTabUseCases
import mozilla.components.feature.share.RecentAppsStorage
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.share.listadapters.AppShareOption

@RunWith(FenixRobolectricTestRunner::class)
class ShareControllerTest {
    // Need a valid context to retrieve Strings for example, but we also need it to return our "metrics"
    private val context: Context = spyk(testContext)
    private val metrics: MetricController = mockk(relaxed = true)
    private val shareSubject = "shareSubject"
    private val shareData = listOf(
        ShareData(url = "url0", title = "title0"),
        ShareData(url = "url1", title = "title1")
    )

    // Navigation between app fragments uses ShareTab as arguments. SendTabUseCases uses TabData.
    private val tabsData = listOf(
        TabData("title0", "url0"),
        TabData("title1", "url1")
    )
    private val textToShare = "${shareData[0].url}\n\n${shareData[1].url}"
    private val sendTabUseCases = mockk<SendTabUseCases>(relaxed = true)
    private val snackbar = mockk<FenixSnackbar>(relaxed = true)
    private val navController = mockk<NavController>(relaxed = true)
    private val dismiss = mockk<(ShareController.Result) -> Unit>(relaxed = true)
    private val recentAppStorage = mockk<RecentAppsStorage>(relaxed = true)

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val testDispatcher = coroutinesTestRule.testDispatcher
    private val testCoroutineScope = TestCoroutineScope(testDispatcher)
    private val controller = DefaultShareController(
        context, shareSubject, shareData, sendTabUseCases, snackbar, navController,
        recentAppStorage, testCoroutineScope, testDispatcher, dismiss
    )

    @Before
    fun setUp() {
        every { context.metrics } returns metrics
    }

    @After
    fun cleanUp() {
        testCoroutineScope.cleanupTestCoroutines()
    }

    @Test
    fun `handleShareClosed should call a passed in delegate to close this`() {
        controller.handleShareClosed()

        verify { dismiss(ShareController.Result.DISMISSED) }
    }

    @Test
    fun `handleShareToApp should start a new sharing activity and close this`() = runBlocking {
        val appPackageName = "package"
        val appClassName = "activity"
        val appShareOption = AppShareOption("app", mockk(), appPackageName, appClassName)
        val shareIntent = slot<Intent>()
        // Our share Intent uses `FLAG_ACTIVITY_NEW_TASK` but when resolving the startActivity call
        // needed for capturing the actual Intent used the `slot` one doesn't have this flag so we
        // need to use an Activity Context.
        val activityContext: Context = mockk<Activity>()
        val testController = DefaultShareController(
            activityContext, shareSubject, shareData, mockk(),
            mockk(), mockk(), recentAppStorage, testCoroutineScope, testDispatcher, dismiss
        )
        every { activityContext.startActivity(capture(shareIntent)) } just Runs
        every { recentAppStorage.updateRecentApp(appShareOption.activityName) } just Runs

        testController.handleShareToApp(appShareOption)
        testDispatcher.advanceUntilIdle()

        // Check that the Intent used for querying apps has the expected structure
        assertTrue(shareIntent.isCaptured)
        assertEquals(Intent.ACTION_SEND, shareIntent.captured.action)
        assertEquals(shareSubject, shareIntent.captured.extras!![Intent.EXTRA_SUBJECT])
        assertEquals(textToShare, shareIntent.captured.extras!![Intent.EXTRA_TEXT])
        assertEquals("text/plain", shareIntent.captured.type)
        assertEquals(Intent.FLAG_ACTIVITY_NEW_DOCUMENT + Intent.FLAG_ACTIVITY_MULTIPLE_TASK, shareIntent.captured.flags)
        assertEquals(appPackageName, shareIntent.captured.component!!.packageName)
        assertEquals(appClassName, shareIntent.captured.component!!.className)

        verify { recentAppStorage.updateRecentApp(appShareOption.activityName) }
        verifyOrder {
            activityContext.startActivity(shareIntent.captured)
            dismiss(ShareController.Result.SUCCESS)
        }
    }

    @Test
    fun `handleShareToApp should dismiss with an error start when a security exception occurs`() {
        val appPackageName = "package"
        val appClassName = "activity"
        val appShareOption = AppShareOption("app", mockk(), appPackageName, appClassName)
        val shareIntent = slot<Intent>()
        // Our share Intent uses `FLAG_ACTIVITY_NEW_TASK` but when resolving the startActivity call
        // needed for capturing the actual Intent used the `slot` one doesn't have this flag so we
        // need to use an Activity Context.
        val activityContext: Context = mockk<Activity>()
        val testController = DefaultShareController(
            activityContext, shareSubject, shareData, mockk(),
            snackbar, mockk(), recentAppStorage, testCoroutineScope, testDispatcher, dismiss
        )
        every { recentAppStorage.updateRecentApp(appShareOption.activityName) } just Runs
        every { activityContext.startActivity(capture(shareIntent)) } throws SecurityException()
        every { activityContext.getString(R.string.share_error_snackbar) } returns "Cannot share to this app"

        testController.handleShareToApp(appShareOption)

        verifyOrder {
            activityContext.startActivity(shareIntent.captured)
            snackbar.setText("Cannot share to this app")
            snackbar.show()
            dismiss(ShareController.Result.SHARE_ERROR)
        }
    }

    @Test
    fun `handleShareToApp should dismiss with an error start when a ActivityNotFoundException occurs`() {
        val appPackageName = "package"
        val appClassName = "activity"
        val appShareOption = AppShareOption("app", mockk(), appPackageName, appClassName)
        val shareIntent = slot<Intent>()
        // Our share Intent uses `FLAG_ACTIVITY_NEW_TASK` but when resolving the startActivity call
        // needed for capturing the actual Intent used the `slot` one doesn't have this flag so we
        // need to use an Activity Context.
        val activityContext: Context = mockk<Activity>()
        val testController = DefaultShareController(
            activityContext, shareSubject, shareData, mockk(),
            snackbar, mockk(), recentAppStorage, testCoroutineScope, testDispatcher, dismiss
        )
        every { recentAppStorage.updateRecentApp(appShareOption.activityName) } just Runs
        every { activityContext.startActivity(capture(shareIntent)) } throws ActivityNotFoundException()
        every { activityContext.getString(R.string.share_error_snackbar) } returns "Cannot share to this app"

        testController.handleShareToApp(appShareOption)

        verifyOrder {
            activityContext.startActivity(shareIntent.captured)
            snackbar.setText("Cannot share to this app")
            snackbar.show()
            dismiss(ShareController.Result.SHARE_ERROR)
        }
    }

    @Test
    fun `getShareSubject should return the shareSubject when shareSubject is not null`() {
        val activityContext: Context = mockk<Activity>()
        val testController = DefaultShareController(
            activityContext, shareSubject, shareData, mockk(),
            mockk(), mockk(), recentAppStorage, testCoroutineScope, testDispatcher, dismiss
        )

        assertEquals(shareSubject, testController.getShareSubject())
    }

    @Test
    fun `getShareSubject should return a combination of non-null titles when shareSubject is null`() {
        val activityContext: Context = mockk<Activity>()
        val testController = DefaultShareController(
            activityContext, null, shareData, mockk(),
            mockk(), mockk(), recentAppStorage, testCoroutineScope, testDispatcher, dismiss
        )

        assertEquals("title0, title1", testController.getShareSubject())
    }

    @Test
    fun `getShareSubject should return just the not null titles string when shareSubject is  null`() {
        val activityContext: Context = mockk<Activity>()
        val partialTitlesShareData = listOf(
            ShareData(url = "url0", title = null),
            ShareData(url = "url1", title = "title1")
        )
        val testController = DefaultShareController(
            activityContext, null, partialTitlesShareData, mockk(),
            mockk(), mockk(), recentAppStorage, testCoroutineScope, testDispatcher, dismiss
        )

        assertEquals("title1", testController.getShareSubject())
    }

    @Test
    fun `getShareSubject should return empty string when shareSubject and all titles are null`() {
        val activityContext: Context = mockk<Activity>()
        val noTitleShareData = listOf(
            ShareData(url = "url0", title = null),
            ShareData(url = "url1", title = null)
        )
        val testController = DefaultShareController(
            activityContext, null, noTitleShareData, mockk(),
            mockk(), mockk(), recentAppStorage, testCoroutineScope, testDispatcher, dismiss
        )

        assertEquals("", testController.getShareSubject())
    }

    @Test
    fun `getShareSubject should return empty string when shareSubject is null and and all titles are empty`() {
        val activityContext: Context = mockk<Activity>()
        val noTitleShareData = listOf(
            ShareData(url = "url0", title = ""),
            ShareData(url = "url1", title = "")
        )
        val testController = DefaultShareController(
            activityContext, null, noTitleShareData, mockk(),
            mockk(), mockk(), recentAppStorage, testCoroutineScope, testDispatcher, dismiss
        )

        assertEquals("", testController.getShareSubject())
    }

    @Test
    @Suppress("DeferredResultUnused")
    fun `handleShareToDevice should share to account device, inform callbacks and dismiss`() {
        val deviceToShareTo = Device(
            "deviceId", "deviceName", DeviceType.UNKNOWN, false, 0L, emptyList(), false, null
        )
        val deviceId = slot<String>()
        val tabsShared = slot<List<TabData>>()

        controller.handleShareToDevice(deviceToShareTo)

        // Verify all the needed methods are called.
        verifyOrder {
            metrics.track(Event.SendTab)
            sendTabUseCases.sendToDeviceAsync(capture(deviceId), capture(tabsShared))
            // dismiss() is also to be called, but at the moment cannot test it in a coroutine.
        }

        assertTrue(deviceId.isCaptured)
        assertEquals(deviceToShareTo.id, deviceId.captured)
        assertTrue(tabsShared.isCaptured)
        assertEquals(tabsData, tabsShared.captured)
    }

    @Test
    @Suppress("DeferredResultUnused")
    fun `handleShareToAllDevices calls handleShareToDevice multiple times`() {
        val devicesToShareTo = listOf(
            Device(
                "deviceId0",
                "deviceName0",
                DeviceType.UNKNOWN,
                false,
                0L,
                emptyList(),
                false,
                null
            ),
            Device(
                "deviceId1",
                "deviceName1",
                DeviceType.UNKNOWN,
                true,
                1L,
                emptyList(),
                false,
                null
            )
        )
        val tabsShared = slot<List<TabData>>()

        controller.handleShareToAllDevices(devicesToShareTo)

        verifyOrder {
            sendTabUseCases.sendToAllAsync(capture(tabsShared))
            // dismiss() is also to be called, but at the moment cannot test it in a coroutine.
        }

        // SendTabUseCases should send a the `shareTabs` mapped to tabData
        assertTrue(tabsShared.isCaptured)
        assertEquals(tabsData, tabsShared.captured)
    }

    @Test
    fun `handleSignIn should navigate to the Sync Fragment and dismiss this one`() {
        controller.handleSignIn()

        verifyOrder {
            metrics.track(Event.SignInToSendTab)
            navController.nav(
                R.id.shareFragment,
                ShareFragmentDirections.actionGlobalTurnOnSync()
            )
            dismiss(ShareController.Result.DISMISSED)
        }
    }

    @Test
    fun `handleReauth should navigate to the Account Problem Fragment and dismiss this one`() {
        controller.handleReauth()

        verifyOrder {
            navController.nav(
                R.id.shareFragment,
                ShareFragmentDirections.actionGlobalAccountProblemFragment()
            )
            dismiss(ShareController.Result.DISMISSED)
        }
    }

    @Test
    fun `showSuccess should show a snackbar with a success message`() {
        val expectedMessage = controller.getSuccessMessage()
        val expectedTimeout = Snackbar.LENGTH_SHORT

        controller.showSuccess()

        verify {
            snackbar.setText(expectedMessage)
            snackbar.setLength(expectedTimeout)
        }
    }

    @Test
    fun `showFailureWithRetryOption should show a snackbar with a retry action`() {
        val expectedMessage = context.getString(R.string.sync_sent_tab_error_snackbar)
        val expectedTimeout = Snackbar.LENGTH_LONG
        val operation: () -> Unit = { println("Hello World") }
        val expectedRetryMessage =
            context.getString(R.string.sync_sent_tab_error_snackbar_action)

        controller.showFailureWithRetryOption(operation)

        verify {
            snackbar.apply {
                setText(expectedMessage)
                setLength(expectedTimeout)
                setAction(expectedRetryMessage, operation)
                setAppropriateBackground(true)
            }
        }
    }

    @Test
    fun `getSuccessMessage should return different strings depending on the number of shared tabs`() {
        val controllerWithOneSharedTab = DefaultShareController(
            context,
            shareSubject,
            listOf(ShareData(url = "url0", title = "title0")),
            mockk(),
            mockk(),
            mockk(),
            mockk(),
            mockk(),
            mockk(),
            mockk()
        )
        val controllerWithMoreSharedTabs = controller
        val expectedTabSharedMessage = context.getString(R.string.sync_sent_tab_snackbar)
        val expectedTabsSharedMessage = context.getString(R.string.sync_sent_tabs_snackbar)

        val tabSharedMessage = controllerWithOneSharedTab.getSuccessMessage()
        val tabsSharedMessage = controllerWithMoreSharedTabs.getSuccessMessage()

        assertNotEquals(tabsSharedMessage, tabSharedMessage)
        assertEquals(expectedTabSharedMessage, tabSharedMessage)
        assertEquals(expectedTabsSharedMessage, tabsSharedMessage)
    }

    @Test
    fun `getShareText should respect concatenate shared tabs urls`() {
        assertEquals(textToShare, controller.getShareText())
    }

    @Test
    fun `getShareText attempts to use original URL for reader pages`() {
        val shareData = listOf(
            ShareData(url = "moz-extension://eb8df45a-895b-4f3a-896a-c0c71ae4/page.html"),
            ShareData(url = "moz-extension://eb8df45a-895b-4f3a-896a-c0c71ae5/page.html?url=url0"),
            ShareData(url = "url1")
        )
        val controller = DefaultShareController(
            context, shareSubject, shareData, sendTabUseCases, snackbar, navController,
            recentAppStorage, testCoroutineScope, testDispatcher, dismiss
        )

        val expectedShareText = "${shareData[0].url}\n\nurl0\n\n${shareData[2].url}"
        assertEquals(expectedShareText, controller.getShareText())
    }

    @Test
    fun `getShareSubject will return 'shareSubject' if that is non null`() {
        assertEquals(shareSubject, controller.getShareSubject())
    }

    @Test
    fun `getShareSubject will return a concatenation of tab titles if 'shareSubject' is null`() {
        val controller = DefaultShareController(
            context, null, shareData, sendTabUseCases, snackbar, navController,
            recentAppStorage, testCoroutineScope, testDispatcher, dismiss
        )

        assertEquals("title0, title1", controller.getShareSubject())
    }

    @Test
    fun `ShareTab#toTabData maps a list of ShareTab to a TabData list`() {
        var tabData: List<TabData>

        with(controller) {
            tabData = shareData.toTabData()
        }

        assertEquals(tabsData, tabData)
    }

    @Test
    fun `ShareTab#toTabData creates a data url from text if no url is specified`() {
        var tabData: List<TabData>
        val expected = listOf(
            TabData(title = "title0", url = ""),
            TabData(title = "title1", url = "data:,Hello%2C%20World!")
        )

        with(controller) {
            tabData = listOf(
                ShareData(title = "title0"),
                ShareData(title = "title1", text = "Hello, World!")
            ).toTabData()
        }

        assertEquals(expected, tabData)
    }
}
