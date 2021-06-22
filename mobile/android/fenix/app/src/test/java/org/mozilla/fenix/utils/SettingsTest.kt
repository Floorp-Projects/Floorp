/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import io.mockk.every
import io.mockk.spyk
import mozilla.components.feature.sitepermissions.SitePermissionsRules
import mozilla.components.feature.sitepermissions.SitePermissionsRules.Action.ALLOWED
import mozilla.components.feature.sitepermissions.SitePermissionsRules.Action.ASK_TO_ALLOW
import mozilla.components.feature.sitepermissions.SitePermissionsRules.Action.BLOCKED
import mozilla.components.feature.sitepermissions.SitePermissionsRules.AutoplayAction
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.settings.deletebrowsingdata.DeleteBrowsingDataOnQuitType
import java.util.Calendar

@RunWith(FenixRobolectricTestRunner::class)
class SettingsTest {

    lateinit var settings: Settings

    private val defaultPermissions = SitePermissionsRules(
        camera = ASK_TO_ALLOW,
        location = ASK_TO_ALLOW,
        microphone = ASK_TO_ALLOW,
        notification = ASK_TO_ALLOW,
        autoplayAudible = AutoplayAction.BLOCKED,
        autoplayInaudible = AutoplayAction.ALLOWED,
        persistentStorage = ASK_TO_ALLOW,
        mediaKeySystemAccess = ASK_TO_ALLOW
    )

    @Before
    fun setUp() {
        settings = Settings(testContext)
    }

    @Test
    fun launchLinksInPrivateTab() {
        // When just created
        // Then
        assertFalse(settings.openLinksInAPrivateTab)

        // When
        settings.openLinksInAPrivateTab = true

        // Then
        assertTrue(settings.openLinksInAPrivateTab)
    }

    @Test
    fun shouldReturnToBrowser() {
        // When just created
        // Then
        assertFalse(settings.shouldReturnToBrowser)

        // When
        settings.shouldReturnToBrowser = true

        // Then
        assertTrue(settings.shouldReturnToBrowser)
    }

    @Test
    fun clearDataOnQuit() {
        // When just created
        // Then
        assertFalse(settings.shouldDeleteBrowsingDataOnQuit)

        // When
        settings.shouldDeleteBrowsingDataOnQuit = true

        // Then
        assertTrue(settings.shouldDeleteBrowsingDataOnQuit)

        // When
        settings.shouldDeleteBrowsingDataOnQuit = false

        // Then
        assertFalse(settings.shouldDeleteBrowsingDataOnQuit)
    }

    @Test
    fun clearAnyDataOnQuit() {
        // When just created
        // Then
        assertFalse(settings.shouldDeleteAnyDataOnQuit())

        // When
        settings.setDeleteDataOnQuit(DeleteBrowsingDataOnQuitType.TABS, true)

        // Then
        assertTrue(settings.shouldDeleteAnyDataOnQuit())

        // When
        settings.setDeleteDataOnQuit(DeleteBrowsingDataOnQuitType.PERMISSIONS, true)

        // Then
        assertTrue(settings.shouldDeleteAnyDataOnQuit())

        // When
        settings.setDeleteDataOnQuit(DeleteBrowsingDataOnQuitType.TABS, false)
        settings.setDeleteDataOnQuit(DeleteBrowsingDataOnQuitType.PERMISSIONS, false)

        // Then
        assertFalse(settings.shouldDeleteAnyDataOnQuit())
    }

    @Test
    fun defaultSearchEngineName() {
        // When just created
        // Then
        assertEquals("", settings.defaultSearchEngineName)

        // When
        settings.defaultSearchEngineName = "Mozilla"

        // Then
        assertEquals("Mozilla", settings.defaultSearchEngineName)
    }

    @Test
    fun isRemoteDebuggingEnabled() {
        // When just created
        // Then
        assertFalse(settings.isRemoteDebuggingEnabled)
    }

    @Test
    fun canShowCfrTest() {
        // When just created
        // Then
        assertEquals(0L, settings.lastCfrShownTimeInMillis)
        assertTrue(settings.canShowCfr)

        // When
        settings.lastCfrShownTimeInMillis = System.currentTimeMillis()

        // Then
        assertFalse(settings.canShowCfr)
    }

    @Test
    fun isTelemetryEnabled() {
        // When just created
        // Then
        assertTrue(settings.isTelemetryEnabled)
    }

    @Test
    fun showLoginsDialogWarningSync() {
        // When just created
        // Then
        assertEquals(0, settings.loginsSecureWarningSyncCount.value)

        // When
        settings.incrementShowLoginsSecureWarningSyncCount()

        // Then
        assertEquals(1, settings.loginsSecureWarningSyncCount.value)
    }

    @Test
    fun shouldShowLoginsDialogWarningSync() {
        // When just created
        // Then
        assertTrue(settings.shouldShowSecurityPinWarningSync)

        // When
        settings.incrementShowLoginsSecureWarningSyncCount()

        // Then
        assertFalse(settings.shouldShowSecurityPinWarningSync)
    }

    @Test
    fun showLoginsDialogWarning() {
        // When just created
        // Then
        assertEquals(0, settings.secureWarningCount.value)

        // When
        settings.incrementSecureWarningCount()

        // Then
        assertEquals(1, settings.secureWarningCount.value)
    }

    @Test
    fun shouldShowLoginsDialogWarning() {
        // When just created
        // Then
        assertTrue(settings.shouldShowSecurityPinWarning)

        // When
        settings.incrementSecureWarningCount()

        // Then
        assertFalse(settings.shouldShowSecurityPinWarning)
    }

    @Test
    fun shouldUseLightTheme() {
        // When just created
        // Then
        assertFalse(settings.shouldUseLightTheme)

        // When
        settings.shouldUseLightTheme = true

        // Then
        assertTrue(settings.shouldUseLightTheme)
    }

    @Test
    fun shouldManuallyCloseTabs() {
        // When just created
        // Then
        assertTrue(settings.manuallyCloseTabs)

        // When
        settings.manuallyCloseTabs = false

        // Then
        assertFalse(settings.shouldUseLightTheme)
    }

    @Test
    fun getTabTimeout() {
        // When just created
        // Then
        assertTrue(settings.manuallyCloseTabs)
        assertEquals(Long.MAX_VALUE, settings.getTabTimeout())

        // When
        settings.manuallyCloseTabs = false
        settings.closeTabsAfterOneDay = true

        // Then
        assertEquals(Settings.ONE_DAY_MS, settings.getTabTimeout())

        // When
        settings.closeTabsAfterOneDay = false
        settings.closeTabsAfterOneWeek = true

        // Then
        assertEquals(Settings.ONE_WEEK_MS, settings.getTabTimeout())

        // When
        settings.closeTabsAfterOneWeek = false
        settings.closeTabsAfterOneMonth = true

        // Then
        assertEquals(Settings.ONE_MONTH_MS, settings.getTabTimeout())
    }

    @Test
    fun shouldUseAutoSize() {
        // When just created
        // Then
        assertTrue(settings.shouldUseAutoSize)

        // When
        settings.shouldUseAutoSize = false

        // Then
        assertFalse(settings.shouldUseAutoSize)
    }

    @Test
    fun shouldAutofill() {
        // When just created
        // Then
        assertTrue(settings.shouldAutofillLogins)

        // When
        settings.shouldAutofillLogins = false

        // Then
        assertFalse(settings.shouldAutofillLogins)
    }

    @Test
    fun fontSizeFactor() {
        // When just created
        // Then
        assertEquals(1f, settings.fontSizeFactor)

        // When
        settings.fontSizeFactor = 2f

        // Then
        assertEquals(2f, settings.fontSizeFactor)
    }

    @Test
    fun shouldShowClipboardSuggestion() {
        // When just created
        // Then
        assertTrue(settings.shouldShowClipboardSuggestions)
    }

    @Test
    fun shouldShowSearchShortcuts() {
        // When just created
        // Then
        assertFalse(settings.shouldShowSearchShortcuts)
    }

    @Test
    fun shouldShowHistorySuggestions() {
        // When just created
        // Then
        assertTrue(settings.shouldShowHistorySuggestions)
    }

    @Test
    fun shouldShowBookmarkSuggestions() {
        // When just created
        // Then
        assertTrue(settings.shouldShowBookmarkSuggestions)
    }

    @Test
    fun shouldUseDarkTheme() {
        // When just created
        // Then
        assertFalse(settings.shouldUseDarkTheme)
    }

    @Test
    fun shouldFollowDeviceTheme() {
        // When just created
        // Then
        assertFalse(settings.shouldFollowDeviceTheme)

        // When
        settings.shouldFollowDeviceTheme = true

        // Then
        assertTrue(settings.shouldFollowDeviceTheme)
    }

    @Test
    fun shouldUseTrackingProtection() {
        // When
        // Then
        assertTrue(settings.shouldUseTrackingProtection)

        // When
        settings.shouldUseTrackingProtection = false

        // Then
        assertFalse(settings.shouldUseTrackingProtection)
    }

    @Test
    fun shouldShowCollectionsPlaceholderOnHome() {
        // When
        // Then
        assertTrue(settings.showCollectionsPlaceholderOnHome)

        // When
        settings.showCollectionsPlaceholderOnHome = false

        // Then
        assertFalse(settings.showCollectionsPlaceholderOnHome)
    }

    @Test
    fun shouldSetOpenInAppOpened() {
        // When
        // Then
        assertFalse(settings.openInAppOpened)

        // When
        settings.openInAppOpened = true

        // Then
        assertTrue(settings.openInAppOpened)
    }

    @Test
    fun shouldSetInstallPwaOpened() {
        // When
        // Then
        assertFalse(settings.installPwaOpened)

        // When
        settings.installPwaOpened = true

        // Then
        assertTrue(settings.installPwaOpened)
    }

    @Test
    fun shouldUseTrackingProtectionStrict() {
        // When
        // Then
        assertFalse(settings.useStrictTrackingProtection)
    }

    @Test
    fun shouldUseAutoBatteryTheme() {
        // When just created
        // Then
        assertFalse(settings.shouldUseAutoBatteryTheme)
    }

    @Test
    fun showSearchSuggestions() {
        // When just created
        // Then
        assertTrue(settings.shouldShowSearchSuggestions)
    }

    @Test
    fun showPwaFragment() {
        // When just created
        // Then
        assertFalse(settings.shouldShowPwaCfr)

        // When visited once
        settings.incrementVisitedInstallableCount()

        // Then
        assertFalse(settings.shouldShowPwaCfr)

        // When visited twice
        settings.incrementVisitedInstallableCount()

        // Then
        assertFalse(settings.shouldShowPwaCfr)

        // When visited thrice
        settings.incrementVisitedInstallableCount()

        // Then
        assertTrue(settings.shouldShowPwaCfr)
    }

    @Test
    fun sitePermissionsPhoneFeatureCameraAction() {
        // When just created
        // Then
        assertEquals(
            ASK_TO_ALLOW,
            settings.getSitePermissionsPhoneFeatureAction(PhoneFeature.CAMERA)
        )

        // When
        settings.setSitePermissionsPhoneFeatureAction(PhoneFeature.CAMERA, BLOCKED)

        // Then
        assertEquals(BLOCKED, settings.getSitePermissionsPhoneFeatureAction(PhoneFeature.CAMERA))
    }

    @Test
    fun sitePermissionsPhoneFeatureMicrophoneAction() {
        // When just created
        // Then
        assertEquals(
            ASK_TO_ALLOW,
            settings.getSitePermissionsPhoneFeatureAction(PhoneFeature.MICROPHONE)
        )

        // When
        settings.setSitePermissionsPhoneFeatureAction(PhoneFeature.MICROPHONE, BLOCKED)

        // Then
        assertEquals(
            BLOCKED,
            settings.getSitePermissionsPhoneFeatureAction(PhoneFeature.MICROPHONE)
        )
    }

    @Test
    fun sitePermissionsPhoneFeatureNotificationAction() {
        // When just created
        // Then
        assertEquals(
            ASK_TO_ALLOW,
            settings.getSitePermissionsPhoneFeatureAction(PhoneFeature.NOTIFICATION)
        )

        // When
        settings.setSitePermissionsPhoneFeatureAction(PhoneFeature.NOTIFICATION, BLOCKED)

        // Then
        assertEquals(
            BLOCKED,
            settings.getSitePermissionsPhoneFeatureAction(PhoneFeature.NOTIFICATION)
        )
    }

    @Test
    fun sitePermissionsPhoneFeatureLocation() {
        // When just created
        // Then
        assertEquals(
            ASK_TO_ALLOW,
            settings.getSitePermissionsPhoneFeatureAction(PhoneFeature.LOCATION)
        )

        // When
        settings.setSitePermissionsPhoneFeatureAction(PhoneFeature.LOCATION, BLOCKED)

        // Then
        assertEquals(BLOCKED, settings.getSitePermissionsPhoneFeatureAction(PhoneFeature.LOCATION))
    }

    @Test
    fun getSitePermissionsCustomSettingsRules_default() {
        // When just created
        // Then
        assertEquals(
            defaultPermissions,
            settings.getSitePermissionsCustomSettingsRules()
        )
    }

    @Test
    fun getSitePermissionsCustomSettingsRules_camera() {
        // When
        settings.setSitePermissionsPhoneFeatureAction(PhoneFeature.CAMERA, BLOCKED)

        // Then
        assertEquals(
            defaultPermissions.copy(camera = BLOCKED),
            settings.getSitePermissionsCustomSettingsRules()
        )
    }

    @Test
    fun getSitePermissionsCustomSettingsRules_notification() {
        // When
        settings.setSitePermissionsPhoneFeatureAction(PhoneFeature.NOTIFICATION, BLOCKED)

        // Then
        assertEquals(
            defaultPermissions.copy(notification = BLOCKED),
            settings.getSitePermissionsCustomSettingsRules()
        )
    }

    @Test
    fun getSitePermissionsCustomSettingsRules_location() {
        // When
        settings.setSitePermissionsPhoneFeatureAction(PhoneFeature.LOCATION, BLOCKED)

        // Then
        assertEquals(
            defaultPermissions.copy(location = BLOCKED),
            settings.getSitePermissionsCustomSettingsRules()
        )
    }

    @Test
    fun getSitePermissionsCustomSettingsRules_microphone() {
        // When
        settings.setSitePermissionsPhoneFeatureAction(PhoneFeature.MICROPHONE, BLOCKED)

        // Then
        assertEquals(
            defaultPermissions.copy(microphone = BLOCKED),
            settings.getSitePermissionsCustomSettingsRules()
        )
    }

    @Test
    fun getSitePermissionsCustomSettingsRules_autoplayAudible() {
        settings.setSitePermissionsPhoneFeatureAction(PhoneFeature.AUTOPLAY_AUDIBLE, ALLOWED)

        assertEquals(
            defaultPermissions.copy(autoplayAudible = AutoplayAction.ALLOWED),
            settings.getSitePermissionsCustomSettingsRules()
        )
    }

    @Test
    fun getSitePermissionsCustomSettingsRules_autoplayInaudible() {
        settings.setSitePermissionsPhoneFeatureAction(PhoneFeature.AUTOPLAY_INAUDIBLE, ALLOWED)

        assertEquals(
            defaultPermissions.copy(autoplayInaudible = AutoplayAction.ALLOWED),
            settings.getSitePermissionsCustomSettingsRules()
        )
    }

    @Test
    fun getSitePermissionsCustomSettingsRules_autoplay_defaults() {
        val settings = Settings(testContext)

        assertEquals(
            AutoplayAction.BLOCKED,
            settings.getSitePermissionsCustomSettingsRules().autoplayAudible
        )

        assertEquals(
            AutoplayAction.ALLOWED,
            settings.getSitePermissionsCustomSettingsRules().autoplayInaudible
        )
    }

    @Test
    fun getSitePermissionsCustomSettingsRules_persistentStorage() {
        settings.setSitePermissionsPhoneFeatureAction(PhoneFeature.PERSISTENT_STORAGE, ALLOWED)

        assertEquals(
            defaultPermissions.copy(persistentStorage = ALLOWED),
            settings.getSitePermissionsCustomSettingsRules()
        )

        settings.setSitePermissionsPhoneFeatureAction(PhoneFeature.PERSISTENT_STORAGE, BLOCKED)

        assertEquals(
            defaultPermissions.copy(persistentStorage = BLOCKED),
            settings.getSitePermissionsCustomSettingsRules()
        )
    }

    @Test
    fun getSitePermissionsCustomSettingsRules_mediaKeySystemAccess() {
        settings.setSitePermissionsPhoneFeatureAction(PhoneFeature.MEDIA_KEY_SYSTEM_ACCESS, ALLOWED)

        assertEquals(
            defaultPermissions.copy(mediaKeySystemAccess = ALLOWED),
            settings.getSitePermissionsCustomSettingsRules()
        )

        settings.setSitePermissionsPhoneFeatureAction(PhoneFeature.MEDIA_KEY_SYSTEM_ACCESS, BLOCKED)

        assertEquals(
            defaultPermissions.copy(mediaKeySystemAccess = BLOCKED),
            settings.getSitePermissionsCustomSettingsRules()
        )
    }

    @Test
    fun overrideAmoCollection() {
        // When just created
        // Then
        assertEquals("", settings.overrideAmoCollection)
        assertFalse(settings.amoCollectionOverrideConfigured())

        // When
        settings.overrideAmoCollection = "testCollection"

        // Then
        assertEquals("testCollection", settings.overrideAmoCollection)
        assertTrue(settings.amoCollectionOverrideConfigured())
    }

    @Test
    fun overrideAmoUser() {
        // When just created
        // Then
        assertEquals("", settings.overrideAmoUser)
        assertFalse(settings.amoCollectionOverrideConfigured())

        // When
        settings.overrideAmoUser = "testAmoUser"

        // Then
        assertEquals("testAmoUser", settings.overrideAmoUser)
        assertTrue(settings.amoCollectionOverrideConfigured())
    }

    @Test
    fun creditCardsSavedCount() {
        // When just created
        // Then
        assertEquals(0, settings.creditCardsSavedCount)

        // When
        settings.creditCardsSavedCount += 1

        // Then
        assertEquals(1, settings.creditCardsSavedCount)

        // When
        settings.creditCardsSavedCount += 1

        // Then
        assertEquals(2, settings.creditCardsSavedCount)
    }

    @Test
    fun `GIVEN startOnHomeAlways is selected WHEN calling shouldStartOnHome THEN return true`() {
        settings.startOnHomeAlways = true
        settings.startOnHomeNever = false
        settings.startOnHomeAfterFourHours = false

        assertTrue(settings.shouldStartOnHome())
    }

    @Test
    fun `GIVEN startOnHomeNever is selected WHEN calling shouldStartOnHome THEN return be false`() {
        settings.startOnHomeNever = true
        settings.startOnHomeAlways = false
        settings.startOnHomeAfterFourHours = false

        assertFalse(settings.shouldStartOnHome())
    }

    @Test
    fun `GIVEN startOnHomeAfterFourHours is selected after four hours of inactivity WHEN calling shouldStartOnHome THEN return true`() {
        val localSetting = spyk(settings)
        val now = Calendar.getInstance()

        localSetting.startOnHomeAfterFourHours = true
        localSetting.startOnHomeNever = false
        localSetting.startOnHomeAlways = false

        now.timeInMillis = System.currentTimeMillis()
        localSetting.lastBrowseActivity = now.timeInMillis
        now.add(Calendar.HOUR, 4)

        every { localSetting.timeNowInMillis() } returns now.timeInMillis

        assertTrue(localSetting.shouldStartOnHome())
    }

    @Test
    fun `GIVEN startOnHomeAfterFourHours is selected and with recent activity WHEN calling shouldStartOnHome THEN return false`() {
        val localSetting = spyk(settings)
        val now = System.currentTimeMillis()

        localSetting.startOnHomeAfterFourHours = true
        localSetting.startOnHomeNever = false
        localSetting.startOnHomeAlways = false

        localSetting.lastBrowseActivity = now

        every { localSetting.timeNowInMillis() } returns now

        assertFalse(localSetting.shouldStartOnHome())
    }

    @Test
    fun `GIVEN shownDefaultBrowserNotification and isDefaultBrowser WHEN calling shouldShowDefaultBrowserNotification THEN return correct value`() {
        val localSetting = spyk(settings)
        every { localSetting.isDefaultBrowserBlocking() } returns false

        localSetting.defaultBrowserNotificationDisplayed = false
        assert(localSetting.shouldShowDefaultBrowserNotification())

        localSetting.defaultBrowserNotificationDisplayed = true
        assertFalse(localSetting.shouldShowDefaultBrowserNotification())

        every { localSetting.isDefaultBrowserBlocking() } returns true

        localSetting.defaultBrowserNotificationDisplayed = false
        assertFalse(localSetting.shouldShowDefaultBrowserNotification())

        localSetting.defaultBrowserNotificationDisplayed = true
        assertFalse(localSetting.shouldShowDefaultBrowserNotification())
    }
}
