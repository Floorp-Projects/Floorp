/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

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

@RunWith(FenixRobolectricTestRunner::class)
class SettingsTest {

    lateinit var settings: Settings

    private val defaultPermissions = SitePermissionsRules(
        camera = ASK_TO_ALLOW,
        location = ASK_TO_ALLOW,
        microphone = ASK_TO_ALLOW,
        notification = ASK_TO_ALLOW,
        autoplayAudible = AutoplayAction.BLOCKED,
        autoplayInaudible = AutoplayAction.BLOCKED
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
    fun isTelemetryEnabled() {
        // When just created
        // Then
        assertTrue(settings.isTelemetryEnabled)
    }

    @Test
    fun showLoginsDialogWarningSync() {
        // When just created
        // Then
        assertEquals(0, settings.loginsSecureWarningSyncCount)

        // When
        settings.incrementShowLoginsSecureWarningSyncCount()

        // Then
        assertEquals(1, settings.loginsSecureWarningSyncCount)
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
        assertEquals(0, settings.loginsSecureWarningCount)

        // When
        settings.incrementShowLoginsSecureWarningCount()

        // Then
        assertEquals(1, settings.loginsSecureWarningCount)
    }

    @Test
    fun shouldShowLoginsDialogWarning() {
        // When just created
        // Then
        assertTrue(settings.shouldShowSecurityPinWarning)

        // When
        settings.incrementShowLoginsSecureWarningCount()

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
        assertFalse(settings.shouldShowPwaOnboarding)

        // When visited once
        settings.incrementVisitedInstallableCount()

        // Then
        assertFalse(settings.shouldShowPwaOnboarding)

        // When visited twice
        settings.incrementVisitedInstallableCount()

        // Then
        assertFalse(settings.shouldShowPwaOnboarding)

        // When visited thrice
        settings.incrementVisitedInstallableCount()

        // Then
        assertTrue(settings.shouldShowPwaOnboarding)
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
            defaultPermissions.copy(autoplayAudible = ALLOWED),
            settings.getSitePermissionsCustomSettingsRules()
        )
    }

    @Test
    fun getSitePermissionsCustomSettingsRules_autoplayInaudible() {
        settings.setSitePermissionsPhoneFeatureAction(PhoneFeature.AUTOPLAY_INAUDIBLE, ALLOWED)

        assertEquals(
            defaultPermissions.copy(autoplayInaudible = ALLOWED),
            settings.getSitePermissionsCustomSettingsRules()
        )
    }
}
