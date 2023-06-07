package org.mozilla.fenix.ui

import android.content.res.Configuration
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.isSoftKeyboardVisible
import org.mozilla.fenix.helpers.TestHelper.verifyDarkThemeApplied
import org.mozilla.fenix.helpers.TestHelper.verifyLightThemeApplied
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

class OnboardingTest {
    private lateinit var mDevice: UiDevice
    private lateinit var mockWebServer: MockWebServer
    private val privacyNoticeLink = "mozilla.org/en-US/privacy/firefox"

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

    private fun getUITheme(): Boolean {
        val mode =
            activityTestRule.activity.resources?.configuration?.uiMode?.and(Configuration.UI_MODE_NIGHT_MASK)

        return when (mode) {
            Configuration.UI_MODE_NIGHT_YES -> true // dark theme is set
            Configuration.UI_MODE_NIGHT_NO -> false // dark theme is not set, using light theme
            else -> false // default option is light theme
        }
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
            verifyPrivacyProtectionCard(
                settings = activityTestRule.activity.getSettings(),
                isStandardChecked = true,
                isStrictChecked = false,
            )
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

    @Ignore("Failing, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1807268")
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
            verifyStartBrowsingButton()
        }.openSearch {
            verifyScanButton()
            verifySearchEngineButton()
            assertTrue(isSoftKeyboardVisible())
        }.dismissSearchBar {
            verifyStartBrowsingButton()
        }
    }

    @Test
    fun dismissOnboardingWithPageLoadTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
            verifyStartBrowsingButton()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.goToHomescreen {
            verifyHomeScreen()
        }
    }

    @Test
    fun chooseYourThemeCardTest() {
        homeScreen {
            verifyChooseYourThemeCard(
                isDarkThemeChecked = false,
                isLightThemeChecked = false,
                isAutomaticThemeChecked = true,
            )
            clickLightThemeButton()
            verifyChooseYourThemeCard(
                isDarkThemeChecked = false,
                isLightThemeChecked = true,
                isAutomaticThemeChecked = false,
            )
            verifyLightThemeApplied(getUITheme())
            clickDarkThemeButton()
            verifyChooseYourThemeCard(
                isDarkThemeChecked = true,
                isLightThemeChecked = false,
                isAutomaticThemeChecked = false,
            )
            verifyDarkThemeApplied(getUITheme())
            clickAutomaticThemeButton()
            verifyChooseYourThemeCard(
                isDarkThemeChecked = false,
                isLightThemeChecked = false,
                isAutomaticThemeChecked = true,
            )
            verifyLightThemeApplied(getUITheme())
        }
    }

    @Test
    fun pickYourToolbarPlacementCardTest() {
        homeScreen {
            verifyToolbarPlacementCard(isBottomChecked = true, isTopChecked = false)
            clickTopToolbarPlacementButton()
            verifyToolbarPosition(defaultPosition = false)
            clickBottomToolbarPlacementButton()
            verifyToolbarPosition(defaultPosition = true)
        }
    }

    @Test
    fun privacyProtectionByDefaultCardTest() {
        homeScreen {
            verifyPrivacyProtectionCard(
                settings = activityTestRule.activity.getSettings(),
                isStandardChecked = true,
                isStrictChecked = false,
            )
            clickStrictTrackingProtectionButton()
            verifyPrivacyProtectionCard(
                settings = activityTestRule.activity.getSettings(),
                isStandardChecked = false,
                isStrictChecked = true,
            )
            clickStandardTrackingProtectionButton()
            verifyPrivacyProtectionCard(
                settings = activityTestRule.activity.getSettings(),
                isStandardChecked = true,
                isStrictChecked = false,
            )
        }
    }

    @Test
    fun pickUpWhereYouLeftOffCardTest() {
        homeScreen {
            verifySignInToSyncCard()
        }.clickSignInButton {
            verifyTurnOnSyncMenu()
        }
    }

    @Test
    fun youControlYourDataCardTest() {
        homeScreen {
            verifyPrivacyNoticeCard()
        }.clickPrivacyNoticeButton {
            verifyCustomTabToolbarTitle("Firefox Privacy Notice")
        }.goBackToOnboardingScreen {
            verifyPrivacyNoticeCard()
        }
    }
}
