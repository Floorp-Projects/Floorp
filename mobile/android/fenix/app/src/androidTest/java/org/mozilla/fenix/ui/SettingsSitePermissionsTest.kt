/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.core.net.toUri
import androidx.test.espresso.Espresso.pressBack
import androidx.test.filters.SdkSuppress
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.mediasession.MediaSession
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.TestAssetHelper.getGenericAsset
import org.mozilla.fenix.helpers.TestAssetHelper.getMutedVideoPageAsset
import org.mozilla.fenix.helpers.TestAssetHelper.getVideoPageAsset
import org.mozilla.fenix.helpers.TestHelper.exitMenu
import org.mozilla.fenix.helpers.TestHelper.grantSystemPermission
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.clickPageObject
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying
 *  - site permissions settings sub-menu
 *  - the settings effects on the app behavior
 *
 */
class SettingsSitePermissionsTest {
    /* Test page created and handled by the Mozilla mobile test-eng team */
    private val permissionsTestPage = "https://mozilla-mobile.github.io/testapp/v2.0/permissions"
    private val permissionsTestPageHost = "https://mozilla-mobile.github.io"
    private val testPageSubstring = "https://mozilla-mobile.github.io:443"
    private lateinit var mockWebServer: MockWebServer
    private lateinit var browserStore: BrowserStore

    @get:Rule
    val activityTestRule = HomeActivityTestRule(
        isJumpBackInCFREnabled = false,
        isPWAsPromptEnabled = false,
        isTCPCFREnabled = false,
        isDeleteSitePermissionsEnabled = true,
    )

    @Before
    fun setUp() {
        // Initializing this as part of class construction, below the rule would throw a NPE
        // So we are initializing this here instead of in all tests.
        browserStore = activityTestRule.activity.components.core.store

        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    // Verifies that you can go to System settings and change app's permissions from inside the app
    @SmokeTest
    @Test
    @SdkSuppress(minSdkVersion = 29)
    fun redirectToAppPermissionsSystemSettingsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuSitePermissions {
        }.openCamera {
            verifyBlockedByAndroidSection()
        }.goBack {
        }.openLocation {
            verifyBlockedByAndroidSection()
        }.goBack {
        }.openMicrophone {
            verifyBlockedByAndroidSection()
            clickGoToSettingsButton()
            openAppSystemPermissionsSettings()
            switchAppPermissionSystemSetting("Camera", "Allow")
            goBackToSystemAppPermissionSettings()
            verifySystemGrantedPermission("Camera")
            switchAppPermissionSystemSetting("Location", "Allow")
            goBackToSystemAppPermissionSettings()
            verifySystemGrantedPermission("Location")
            switchAppPermissionSystemSetting("Microphone", "Allow")
            goBackToSystemAppPermissionSettings()
            verifySystemGrantedPermission("Microphone")
            goBackToPermissionsSettingsSubMenu()
            verifyUnblockedByAndroid()
        }.goBack {
        }.openLocation {
            verifyUnblockedByAndroid()
        }.goBack {
        }.openCamera {
            verifyUnblockedByAndroid()
        }
    }

    @SmokeTest
    @Test
    fun verifyAutoplayBlockAudioOnlySettingTest() {
        val genericPage = getGenericAsset(mockWebServer, 1)
        val videoTestPage = getVideoPageAsset(mockWebServer)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuSitePermissions {
        }.openAutoPlay {
            verifySitePermissionsAutoPlaySubMenuItems()
            exitMenu()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericPage.url) {
            verifyPageContent(genericPage.content)
        }.openTabDrawer {
            closeTab()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(videoTestPage.url) {
            try {
                verifyPageContent(videoTestPage.content)
                clickPageObject(itemWithText("Play"))
                assertPlaybackState(browserStore, MediaSession.PlaybackState.PLAYING)
            } catch (e: java.lang.AssertionError) {
                navigationToolbar {
                }.openThreeDotMenu {
                }.refreshPage {
                    verifyPageContent(videoTestPage.content)
                    clickPageObject(itemWithText("Play"))
                    assertPlaybackState(browserStore, MediaSession.PlaybackState.PLAYING)
                }
            }
        }
    }

    @Ignore("Failing, see https://bugzilla.mozilla.org/show_bug.cgi?id=1827599")
    @SmokeTest
    @Test
    fun verifyAutoplayBlockAudioOnlySettingOnMutedVideoTest() {
        val genericPage = getGenericAsset(mockWebServer, 1)
        val mutedVideoTestPage = getMutedVideoPageAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericPage.url) {
            verifyPageContent(genericPage.content)
        }.openTabDrawer {
            closeTab()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(mutedVideoTestPage.url) {
            try {
                verifyPageContent("Media file is playing")
            } catch (e: java.lang.AssertionError) {
                navigationToolbar {
                }.openThreeDotMenu {
                }.refreshPage {
                    verifyPageContent("Media file is playing")
                }
            }
        }
    }

    @Test
    fun verifyAutoplayAllowAudioVideoSettingTest() {
        val genericPage = getGenericAsset(mockWebServer, 1)
        val videoTestPage = getVideoPageAsset(mockWebServer)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuSitePermissions {
        }.openAutoPlay {
            selectAutoplayOption("Allow audio and video")
            exitMenu()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericPage.url) {
            verifyPageContent(genericPage.content)
        }.openTabDrawer {
            closeTab()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(videoTestPage.url) {
            try {
                verifyPageContent(videoTestPage.content)
                assertPlaybackState(browserStore, MediaSession.PlaybackState.PLAYING)
            } catch (e: java.lang.AssertionError) {
                navigationToolbar {
                }.openThreeDotMenu {
                }.refreshPage {
                    verifyPageContent(videoTestPage.content)
                    assertPlaybackState(browserStore, MediaSession.PlaybackState.PLAYING)
                }
            }
        }
    }

    @Ignore("Failing, see https://bugzilla.mozilla.org/show_bug.cgi?id=1827599")
    @Test
    fun verifyAutoplayAllowAudioVideoSettingOnMutedVideoTest() {
        val mutedVideoTestPage = getMutedVideoPageAsset(mockWebServer)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuSitePermissions {
        }.openAutoPlay {
            selectAutoplayOption("Allow audio and video")
            exitMenu()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(mutedVideoTestPage.url) {
            try {
                verifyPageContent("Media file is playing")
            } catch (e: java.lang.AssertionError) {
                navigationToolbar {
                }.openThreeDotMenu {
                }.refreshPage {
                    verifyPageContent("Media file is playing")
                }
            }
        }
    }

    @Test
    fun verifyAutoplayBlockAudioAndVideoSettingTest() {
        val videoTestPage = getVideoPageAsset(mockWebServer)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuSitePermissions {
        }.openAutoPlay {
            selectAutoplayOption("Block audio and video")
            exitMenu()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(videoTestPage.url) {
            try {
                verifyPageContent(videoTestPage.content)
                clickPageObject(itemWithText("Play"))
                assertPlaybackState(browserStore, MediaSession.PlaybackState.PLAYING)
            } catch (e: java.lang.AssertionError) {
                navigationToolbar {
                }.openThreeDotMenu {
                }.refreshPage {
                    verifyPageContent(videoTestPage.content)
                    clickPageObject(itemWithText("Play"))
                    assertPlaybackState(browserStore, MediaSession.PlaybackState.PLAYING)
                }
            }
        }
    }

    @Test
    fun verifyAutoplayBlockAudioAndVideoSettingOnMutedVideoTest() {
        val mutedVideoTestPage = getMutedVideoPageAsset(mockWebServer)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuSitePermissions {
        }.openAutoPlay {
            selectAutoplayOption("Block audio and video")
            exitMenu()
        }
        navigationToolbar {
        }.enterURLAndEnterToBrowser(mutedVideoTestPage.url) {
            verifyPageContent("Media file not playing")
            clickPageObject(itemWithText("Play"))
            try {
                verifyPageContent("Media file is playing")
            } catch (e: java.lang.AssertionError) {
                navigationToolbar {
                }.openThreeDotMenu {
                }.refreshPage {
                    clickPageObject(itemWithText("Play"))
                    verifyPageContent("Media file is playing")
                }
            }
        }
    }

    @Test
    fun verifyCameraPermissionSettingsTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(permissionsTestPage.toUri()) {
        }.clickStartCameraButton {
            grantSystemPermission()
            verifyCameraPermissionPrompt(testPageSubstring)
            pressBack()
        }
        browserScreen {
            navigationToolbar {
            }.openThreeDotMenu {
            }.openSettings {
            }.openSettingsSubMenuSitePermissions {
            }.openCamera {
                verifySitePermissionsCommonSubMenuItems()
                selectPermissionSettingOption("Blocked")
                exitMenu()
            }
        }.clickStartCameraButton {}
        browserScreen {
            verifyPageContent("Camera not allowed")
        }
    }

    @Test
    fun verifyMicrophonePermissionSettingsTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(permissionsTestPage.toUri()) {
        }.clickStartMicrophoneButton {
            grantSystemPermission()
            verifyMicrophonePermissionPrompt(testPageSubstring)
            pressBack()
        }
        browserScreen {
            navigationToolbar {
            }.openThreeDotMenu {
            }.openSettings {
            }.openSettingsSubMenuSitePermissions {
            }.openMicrophone {
                verifySitePermissionsCommonSubMenuItems()
                selectPermissionSettingOption("Blocked")
                exitMenu()
            }
        }.clickStartMicrophoneButton {}
        browserScreen {
            verifyPageContent("Microphone not allowed")
        }
    }

    @Test
    fun verifyLocationPermissionSettingsTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(permissionsTestPage.toUri()) {
        }.clickGetLocationButton {
            verifyLocationPermissionPrompt(testPageSubstring)
            pressBack()
        }
        browserScreen {
            navigationToolbar {
            }.openThreeDotMenu {
            }.openSettings {
            }.openSettingsSubMenuSitePermissions {
            }.openLocation {
                verifySitePermissionsCommonSubMenuItems()
                selectPermissionSettingOption("Blocked")
                exitMenu()
            }
        }.clickGetLocationButton {}
        browserScreen {
            verifyPageContent("User denied geolocation prompt")
        }
    }

    @Test
    fun verifyNotificationsPermissionSettingsTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(permissionsTestPage.toUri()) {
        }.clickOpenNotificationButton {
            verifyNotificationsPermissionPrompt(testPageSubstring)
            pressBack()
        }
        browserScreen {
            navigationToolbar {
            }.openThreeDotMenu {
            }.openSettings {
            }.openSettingsSubMenuSitePermissions {
            }.openNotification {
                verifyNotificationSubMenuItems()
                selectPermissionSettingOption("Blocked")
                exitMenu()
            }
        }.clickOpenNotificationButton {}
        browserScreen {
            verifyPageContent("Notifications not allowed")
        }
    }

    @Test
    fun verifyPersistentStoragePermissionSettingsTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(permissionsTestPage.toUri()) {
        }.clickRequestPersistentStorageAccessButton {
            verifyPersistentStoragePermissionPrompt(testPageSubstring)
            pressBack()
        }
        browserScreen {
            navigationToolbar {
            }.openThreeDotMenu {
            }.openSettings {
            }.openSettingsSubMenuSitePermissions {
            }.openPersistentStorage {
                verifySitePermissionsPersistentStorageSubMenuItems()
                selectPermissionSettingOption("Blocked")
                exitMenu()
            }
        }.clickRequestPersistentStorageAccessButton {}
        browserScreen {
            verifyPageContent("Persistent storage permission denied")
        }
    }

    @Ignore("Flaky, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1829889")
    @Test
    fun verifyDRMControlledContentPermissionSettingsTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(permissionsTestPage.toUri()) {
        }.clickRequestDRMControlledContentAccessButton {
            verifyDRMContentPermissionPrompt(testPageSubstring)
            pressBack()
            browserScreen {
            }.openThreeDotMenu {
            }.openSettings {
            }.openSettingsSubMenuSitePermissions {
            }.openDRMControlledContent {
                verifyDRMControlledContentSubMenuItems()
                selectDRMControlledContentPermissionSettingOption("Blocked")
                exitMenu()
            }
            browserScreen {
            }.clickRequestDRMControlledContentAccessButton {}
            browserScreen {
                verifyPageContent("DRM-controlled content not allowed")
            }.openThreeDotMenu {
            }.openSettings {
            }.openSettingsSubMenuSitePermissions {
            }.openDRMControlledContent {
                selectDRMControlledContentPermissionSettingOption("Allowed")
                exitMenu()
            }
            browserScreen {
            }.openThreeDotMenu {
            }.refreshPage {
            }.clickRequestDRMControlledContentAccessButton {}
            browserScreen {
                verifyPageContent("DRM-controlled content allowed")
            }
        }
    }

    @SmokeTest
    @Test
    fun clearAllSitePermissionsExceptionsTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(permissionsTestPage.toUri()) {
        }.clickOpenNotificationButton {
            verifyNotificationsPermissionPrompt(testPageSubstring)
        }.clickPagePermissionButton(true) {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuSitePermissions {
        }.openExceptions {
            verifyExceptionCreated(permissionsTestPageHost, true)
            clickClearPermissionsOnAllSites()
            verifyClearPermissionsDialog()
            clickCancel()
            clickClearPermissionsOnAllSites()
            clickOK()
            verifyExceptionsEmptyList()
        }
    }

    @Test
    fun clearOneSiteAllPermissionsExceptionsTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(permissionsTestPage.toUri()) {
        }.clickOpenNotificationButton {
            verifyNotificationsPermissionPrompt(testPageSubstring)
        }.clickPagePermissionButton(true) {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuSitePermissions {
        }.openExceptions {
            verifyExceptionCreated(permissionsTestPageHost, true)
            openSiteExceptionsDetails(permissionsTestPageHost)
            clickClearPermissionsForOneSite()
            verifyClearPermissionsForOneSiteDialog()
            clickCancel()
            clickClearPermissionsForOneSite()
            clickOK()
            verifyExceptionsEmptyList()
        }
    }

    @Test
    fun clearOneSiteOnePermissionExceptionTest() {
        navigationToolbar {
        }.enterURLAndEnterToBrowser(permissionsTestPage.toUri()) {
        }.clickOpenNotificationButton {
            verifyNotificationsPermissionPrompt(testPageSubstring)
        }.clickPagePermissionButton(true) {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSettingsSubMenuSitePermissions {
        }.openExceptions {
            verifyExceptionCreated(permissionsTestPageHost, true)
            openSiteExceptionsDetails(permissionsTestPageHost)
            verifyPermissionSettingSummary("Notification", "Allowed")
            openChangePermissionSettingsMenu("Notification")
            clickClearOnePermissionForOneSite()
            verifyResetPermissionDefaultForThisSiteDialog()
            clickOK()
            pressBack()
            verifyPermissionSettingSummary("Notification", "Ask to allow")
            pressBack()
            // This should be changed to false, when https://bugzilla.mozilla.org/show_bug.cgi?id=1826297 is fixed
            verifyExceptionCreated(permissionsTestPageHost, true)
        }
    }
}
