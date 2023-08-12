/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import mozilla.components.concept.engine.utils.EngineReleaseChannel
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.RecyclerViewIdlingResource
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.helpers.TestHelper.runWithCondition
import org.mozilla.fenix.ui.robots.navigationToolbar

class MainMenuTest {
    private lateinit var mDevice: UiDevice
    private lateinit var mockWebServer: MockWebServer

    @get:Rule
    val activityTestRule = HomeActivityTestRule.withDefaultSettingsOverrides()

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

    @SmokeTest
    @Test
    fun verifyPageMainMenuItemsTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            waitForPageToLoad()
        }.openThreeDotMenu {
            verifyPageThreeDotMainMenuItems(isRequestDesktopSiteEnabled = false)
        }
    }

    @SmokeTest
    @Test
    fun openMainMenuNewTabItemTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.clickNewTabButton {
            verifySearchView()
        }
    }

    @SmokeTest
    @Test
    fun openMainMenuBookmarksItemTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.openBookmarks {
            verifyBookmarksMenuView()
        }
    }

    @SmokeTest
    @Test
    fun openMainMenuHistoryItemTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.openHistory {
            verifyHistoryListExists()
        }
    }

    @SmokeTest
    @Test
    fun openMainMenuAddonsTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.openAddonsManagerMenu {
            TestHelper.registerAndCleanupIdlingResources(
                RecyclerViewIdlingResource(
                    activityTestRule.activity.findViewById(R.id.add_ons_list),
                    1,
                ),
            ) {
                verifyAddonsItems()
            }
        }
    }

    @SmokeTest
    @Test
    fun openMainMenuSyncItemTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            mDevice.waitForIdle()
        }.openThreeDotMenu {
        }.openSyncSignIn {
            verifyTurnOnSyncMenu()
        }
    }

    @SmokeTest
    @Test
    fun openMainMenuFindInPageTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.openFindInPage {
            verifyFindInPageSearchBarItems()
        }
    }

    @SmokeTest
    @Test
    fun mainMenuDesktopSiteTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.switchDesktopSiteMode {
        }.openThreeDotMenu {
            verifyDesktopSiteModeEnabled(true)
        }
    }

    @SmokeTest
    @Test
    fun mainMenuReportSiteIssueTest() {
        runWithCondition(
            // This test will not run on RC builds because the "Report site issue button" is not available.
            activityTestRule.activity.components.core.engine.version.releaseChannel !== EngineReleaseChannel.RELEASE,
        ) {
            val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

            navigationToolbar {
            }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            }.openThreeDotMenu {
            }.openReportSiteIssue {
                verifyUrl("webcompat.com/issues/new")
            }
        }
    }

    @SmokeTest
    @Test
    fun openMainMenuAddToCollectionTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.openSaveToCollection {
            verifyCollectionNameTextField()
        }
    }

    // Test running on beta/release builds in CI:
    // caution when making changes to it, so they don't block the builds
    @SmokeTest
    @Test
    fun openMainMenuSettingsItemTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.openSettings {
            verifySettingsView()
        }
    }

    @SmokeTest
    @Test
    fun mainMenuShareButtonTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openThreeDotMenu {
        }.clickShareButton {
            verifyShareTabLayout()
        }
    }

    @SmokeTest
    @Test
    fun mainMenuRefreshButtonTest() {
        val refreshWebPage = TestAssetHelper.getRefreshAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(refreshWebPage.url) {
            mDevice.waitForIdle()
        }.openThreeDotMenu {
            verifyThreeDotMenuExists()
        }.refreshPage {
            verifyPageContent("REFRESHED")
        }
    }

    @SmokeTest
    @Test
    fun mainMenuForceRefreshTest() {
        val refreshWebPage = TestAssetHelper.getRefreshAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(refreshWebPage.url) {
            mDevice.waitForIdle()
        }.openThreeDotMenu {
            verifyThreeDotMenuExists()
        }.forceRefreshPage {
            verifyPageContent("REFRESHED")
        }
    }
}
