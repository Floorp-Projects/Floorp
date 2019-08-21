/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.feature.sitepermissions.SitePermissionsRules
import mozilla.components.feature.sitepermissions.SitePermissionsRules.Action.ASK_TO_ALLOW
import mozilla.components.feature.sitepermissions.SitePermissionsRules.Action.BLOCKED
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.TestApplication
import org.mozilla.fenix.ext.clearAndCommit
import org.mozilla.fenix.settings.PhoneFeature
import org.robolectric.annotation.Config

@ObsoleteCoroutinesApi
@RunWith(AndroidJUnit4::class)
@Config(application = TestApplication::class)
class SettingsTest {

    lateinit var settings: Settings

    @Before
    fun setUp() {
        settings = Settings.getInstance(testContext)
            .apply(Settings::clear)
    }

    @Test
    fun usePrivateMode() {
        // When just created
        // Then
        assertFalse(settings.usePrivateMode)

        // When
        settings.usePrivateMode = true

        // Then
        assertTrue(settings.usePrivateMode)

        // When
        settings.usePrivateMode = false

        // Then
        assertFalse(settings.usePrivateMode)
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
    fun isCrashReportingEnabled_enabledInBuild() {
        // When
        clearExistingInstance()
        val settings = Settings.getInstance(testContext, isCrashReportEnabledInBuild = true)
            .apply(Settings::clear)

        // Then
        assertTrue(settings.isCrashReportingEnabled)
    }

    @Test
    fun isCrashReportingEnabled_disabledInBuild() {
        // When
        clearExistingInstance()
        val settings = Settings.getInstance(testContext, isCrashReportEnabledInBuild = false)
            .apply(Settings::clear)

        // Then
        assertFalse(settings.isCrashReportingEnabled)
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
    fun autoBounceQuickActionSheetCount() {
        // When just created
        // Then
        assertEquals(0, settings.autoBounceQuickActionSheetCount)

        // When
        settings.incrementAutomaticBounceQuickActionSheetCount()
        settings.incrementAutomaticBounceQuickActionSheetCount()

        // Then
        assertEquals(2, settings.autoBounceQuickActionSheetCount)
    }

    @Test
    fun shouldAutoBounceQuickActionSheet() {
        // When just created
        // Then
        assertTrue(settings.shouldAutoBounceQuickActionSheet)

        // When
        settings.incrementAutomaticBounceQuickActionSheetCount()

        // Then
        assertTrue(settings.shouldAutoBounceQuickActionSheet)

        // When
        settings.incrementAutomaticBounceQuickActionSheetCount()

        // Then
        assertFalse(settings.shouldAutoBounceQuickActionSheet)
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
    fun shouldShowVisitedSitesBookmarks() {
        // When just created
        // Then
        assertTrue(settings.shouldShowVisitedSitesBookmarks)
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
    fun shouldUseAutoBatteryTheme() {
        // When just created
        // Then
        assertFalse(settings.shouldUseAutoBatteryTheme)
    }

    @Test
    fun showSearchSuggestions() {
        // When just created
        // Then
        assertTrue(settings.showSearchSuggestions)
    }

    @Test
    fun sitePermissionsPhoneFeatureCameraAction() {
        // When just created
        // Then
        assertEquals(ASK_TO_ALLOW, settings.getSitePermissionsPhoneFeatureAction(PhoneFeature.CAMERA))

        // When
        settings.setSitePermissionsPhoneFeatureAction(PhoneFeature.CAMERA, BLOCKED)

        // Then
        assertEquals(BLOCKED, settings.getSitePermissionsPhoneFeatureAction(PhoneFeature.CAMERA))
    }

    @Test
    fun sitePermissionsPhoneFeatureMicrophoneAction() {
        // When just created
        // Then
        assertEquals(ASK_TO_ALLOW, settings.getSitePermissionsPhoneFeatureAction(PhoneFeature.MICROPHONE))

        // When
        settings.setSitePermissionsPhoneFeatureAction(PhoneFeature.MICROPHONE, BLOCKED)

        // Then
        assertEquals(BLOCKED, settings.getSitePermissionsPhoneFeatureAction(PhoneFeature.MICROPHONE))
    }

    @Test
    fun sitePermissionsPhoneFeatureNotificationAction() {
        // When just created
        // Then
        assertEquals(ASK_TO_ALLOW, settings.getSitePermissionsPhoneFeatureAction(PhoneFeature.NOTIFICATION))

        // When
        settings.setSitePermissionsPhoneFeatureAction(PhoneFeature.NOTIFICATION, BLOCKED)

        // Then
        assertEquals(BLOCKED, settings.getSitePermissionsPhoneFeatureAction(PhoneFeature.NOTIFICATION))
    }

    @Test
    fun sitePermissionsPhoneFeatureLocation() {
        // When just created
        // Then
        assertEquals(ASK_TO_ALLOW, settings.getSitePermissionsPhoneFeatureAction(PhoneFeature.LOCATION))

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
            allAskToAllow(),
            settings.getSitePermissionsCustomSettingsRules()
        )
    }

    @Test
    fun getSitePermissionsCustomSettingsRules_camera() {
        // When
        settings.setSitePermissionsPhoneFeatureAction(PhoneFeature.CAMERA, BLOCKED)

        // Then
        assertEquals(
            allAskToAllow().copy(camera = BLOCKED),
            settings.getSitePermissionsCustomSettingsRules()
        )
    }

    @Test
    fun getSitePermissionsCustomSettingsRules_notification() {
        // When
        settings.setSitePermissionsPhoneFeatureAction(PhoneFeature.NOTIFICATION, BLOCKED)

        // Then
        assertEquals(
            allAskToAllow().copy(notification = BLOCKED),
            settings.getSitePermissionsCustomSettingsRules()
        )
    }

    @Test
    fun getSitePermissionsCustomSettingsRules_location() {
        // When
        settings.setSitePermissionsPhoneFeatureAction(PhoneFeature.LOCATION, BLOCKED)

        // Then
        assertEquals(
            allAskToAllow().copy(location = BLOCKED),
            settings.getSitePermissionsCustomSettingsRules()
        )
    }

    @Test
    fun getSitePermissionsCustomSettingsRules_microphone() {
        // When
        settings.setSitePermissionsPhoneFeatureAction(PhoneFeature.MICROPHONE, BLOCKED)

        // Then
        assertEquals(
            allAskToAllow().copy(microphone = BLOCKED),
            settings.getSitePermissionsCustomSettingsRules()
        )
    }
}

private fun clearExistingInstance() {
    Settings.instance = null
}

private fun Settings.clear() {
    preferences.clearAndCommit()
}

private fun allAskToAllow() = SitePermissionsRules(
    camera = ASK_TO_ALLOW,
    location = ASK_TO_ALLOW,
    microphone = ASK_TO_ALLOW,
    notification = ASK_TO_ALLOW
)
