package org.mozilla.fenix.ui

import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

class OnboardingTest {
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

    // Verifies the first run onboarding screen
    @SmokeTest
    @Test
    fun firstRunScreenTest() {
        homeScreen {
            verifyHomeScreenAppBarItems()
            verifyHomeScreenWelcomeItems()
            verifyChooseYourThemeCard(
                isDarkThemeChecked = false,
                isLightThemeChecked = false,
                isAutomaticThemeChecked = true,
            )
            verifyToolbarPlacementCard(isBottomChecked = true, isTopChecked = false)
            verifySignInToSyncCard()
            verifyPrivacyProtectionCard(isStandardChecked = true, isStrictChecked = false)
            verifyPrivacyNoticeCard()
            verifyStartBrowsingSection()
            verifyNavigationToolbarItems("0")
        }
    }

    // Verifies the functionality of the onboarding Start Browsing button
    @SmokeTest
    @Test
    fun startBrowsingButtonTest() {
        homeScreen {
            verifyStartBrowsingButton()
        }.clickStartBrowsingButton {
            verifySearchView()
        }
    }

    @Test
    fun dismissOnboardingUsingSettingsTest() {
        homeScreen {
            verifyWelcomeHeader()
        }.openThreeDotMenu {
        }.openSettings {
            verifyGeneralHeading()
        }.goBack {
            verifyExistingTopSitesList()
        }
    }

    @Test
    fun dismissOnboardingUsingBookmarksTest() {
        homeScreen {
            verifyWelcomeHeader()
        }.openThreeDotMenu {
        }.openBookmarks {
            verifyBookmarksMenuView()
            navigateUp()
        }
        homeScreen {
            verifyExistingTopSitesList()
        }
    }

    @Test
    fun dismissOnboardingUsingHelpTest() {
        homeScreen {
            verifyWelcomeHeader()
        }.openThreeDotMenu {
        }.openHelp {
            verifyHelpUrl()
        }.goBack {
            verifyExistingTopSitesList()
        }
    }

    @Test
    fun toolbarTapDoesntDismissOnboardingTest() {
        homeScreen {
            verifyWelcomeHeader()
        }.openSearch {
            verifyScanButton()
            verifySearchEngineButton()
            verifyKeyboardVisibility()
        }.dismissSearchBar {
            verifyWelcomeHeader()
        }
    }

    @Test
    fun dismissOnboardingWithPageLoadTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
            verifyWelcomeHeader()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.goToHomescreen {
            verifyExistingTopSitesList()
        }
    }
}
