package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.getStringResource
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.ui.robots.clickPageObject
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

class CrashReportingTest {
    private lateinit var mDevice: UiDevice
    private lateinit var mockWebServer: MockWebServer
    private val tabCrashMessage = getStringResource(R.string.tab_crash_title_2)

    @get:Rule
    val activityTestRule = AndroidComposeTestRule(
        HomeActivityIntentTestRule(
            isPocketEnabled = false,
            isJumpBackInCFREnabled = false,
            isWallpaperOnboardingEnabled = false,
            isTCPCFREnabled = false,
        ),
    ) { it.activity }

    @Before
    fun setUp() {
        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    @Test
    fun closeTabCrashedReporterTest() {
        homeScreen {
        }.openNavigationToolbar {
        }.openTabCrashReporter {
        }.clickTabCrashedCloseButton {
        }.openTabDrawer {
            verifyNoOpenTabsInNormalBrowsing()
        }
    }

    @Ignore("Test failure caused by: https://github.com/mozilla-mobile/fenix/issues/19964")
    @Test
    fun restoreTabCrashedReporterTest() {
        val website = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(website.url) {}

        navigationToolbar {
        }.openTabCrashReporter {
            clickPageObject(itemWithResId("$packageName:id/restoreTabButton"))
            verifyPageContent(website.content)
        }
    }

    @Ignore("Failure: https://bugzilla.mozilla.org/show_bug.cgi?id=1812075")
    @SmokeTest
    @Test
    fun useAppWhileTabIsCrashedTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val secondWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)

        homeScreen {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
            mDevice.waitForIdle()
        }.openTabDrawer {
        }.openNewTab {
        }.submitQuery(secondWebPage.url.toString()) {
            waitForPageToLoad()
        }

        navigationToolbar {
        }.openTabCrashReporter {
            verifyPageContent(tabCrashMessage)
        }.openTabDrawer {
            verifyExistingOpenTabs(firstWebPage.title)
            verifyExistingOpenTabs(secondWebPage.title)
        }.closeTabDrawer {
        }.goToHomescreen {
            verifyExistingTopSitesList()
        }.openThreeDotMenu {
            verifySettingsButton()
        }
    }

    @SmokeTest
    @Test
    fun privateBrowsingUseAppWhileTabIsCrashedTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val secondWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)

        homeScreen {
            togglePrivateBrowsingModeOnOff()
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
            mDevice.waitForIdle()
        }.openTabDrawer {
        }.openNewTab {
        }.submitQuery(secondWebPage.url.toString()) {
            waitForPageToLoad()
        }

        navigationToolbar {
        }.openTabCrashReporter {
            verifyPageContent(tabCrashMessage)
        }.openTabDrawer {
            verifyExistingOpenTabs(firstWebPage.title)
            verifyExistingOpenTabs(secondWebPage.title)
        }.closeTabDrawer {
        }.goToHomescreen {
            verifyCommonMythsLink()
        }.openThreeDotMenu {
            verifySettingsButton()
        }
    }
}
