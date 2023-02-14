/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import kotlinx.coroutines.test.runTest
import mozilla.components.feature.sitepermissions.SitePermissionsRules
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.trackingprotection.ProtectionsState
import org.mozilla.fenix.trackingprotection.ProtectionsState.Mode.Normal

class QuickSettingsFragmentReducerTest {

    @Test
    fun `WebsitePermissionAction - TogglePermission`() {
        val toggleablePermission = WebsitePermission.Toggleable(
            phoneFeature = PhoneFeature.CAMERA,
            status = "status",
            isVisible = false,
            isEnabled = false,
            isBlockedByAndroid = false,
        )

        val map =
            mapOf<PhoneFeature, WebsitePermission>(PhoneFeature.CAMERA to toggleablePermission)
        val infoState = WebsiteInfoState("", "", WebsiteSecurityUiValues.SECURE, "")
        val tpState = ProtectionsState(
            null,
            "",
            isTrackingProtectionEnabled = false,
            isCookieBannerHandlingEnabled = false,
            listTrackers = emptyList(),
            mode = Normal,
            lastAccessedCategory = "",
        )
        val state = QuickSettingsFragmentState(infoState, map, tpState)
        val newState = quickSettingsFragmentReducer(
            state,
            WebsitePermissionAction.TogglePermission(
                updatedFeature = PhoneFeature.CAMERA,
                updatedStatus = "newStatus",
                updatedEnabledStatus = true,
            ),
        )
        val result = newState.websitePermissionsState[PhoneFeature.CAMERA]!!
        assertEquals("newStatus", result.status)
        assertTrue(result.isEnabled)
    }

    @Test
    fun `WebsitePermissionAction - ChangeAutoplay`() {
        val permissionPermission = WebsitePermission.Autoplay(
            autoplayValue = AutoplayValue.BlockAll(
                label = "label",
                rules = createTestRule(),
                sitePermission = null,
            ),
            options = emptyList(),
            isVisible = false,
        )

        val map =
            mapOf<PhoneFeature, WebsitePermission>(PhoneFeature.AUTOPLAY to permissionPermission)
        val infoState = WebsiteInfoState("", "", WebsiteSecurityUiValues.SECURE, "")
        val tpState = ProtectionsState(
            null,
            "",
            isTrackingProtectionEnabled = false,
            isCookieBannerHandlingEnabled = false,
            listTrackers = emptyList(),
            mode = Normal,
            lastAccessedCategory = "",
        )
        val state = QuickSettingsFragmentState(infoState, map, tpState)
        val autoplayValue = AutoplayValue.AllowAll(
            label = "newLabel",
            rules = createTestRule(),
            sitePermission = null,
        )
        val newState = quickSettingsFragmentReducer(
            state,
            WebsitePermissionAction.ChangeAutoplay(autoplayValue),
        )

        val result =
            newState.websitePermissionsState[PhoneFeature.AUTOPLAY] as WebsitePermission.Autoplay
        assertEquals(autoplayValue, result.autoplayValue)
    }

    @Test
    fun `ProtectionsAction - ToggleTrackingProtectionEnabled`() = runTest {
        val state = QuickSettingsFragmentState(
            webInfoState = WebsiteInfoState("", "", WebsiteSecurityUiValues.SECURE, ""),
            websitePermissionsState = emptyMap(),
            protectionsState = ProtectionsState(
                tab = null,
                url = "https://www.firefox.com",
                isTrackingProtectionEnabled = true,
                isCookieBannerHandlingEnabled = true,
                listTrackers = listOf(),
                mode = Normal,
                lastAccessedCategory = "",
            ),
        )

        val newState = quickSettingsFragmentReducer(
            state = state,
            action = TrackingProtectionAction.ToggleTrackingProtectionEnabled(false),
        )

        assertNotSame(state, newState)
        assertFalse(newState.protectionsState.isTrackingProtectionEnabled)
    }

    private fun createTestRule() = SitePermissionsRules(
        SitePermissionsRules.Action.ALLOWED,
        SitePermissionsRules.Action.ALLOWED,
        SitePermissionsRules.Action.ALLOWED,
        SitePermissionsRules.Action.ALLOWED,
        SitePermissionsRules.AutoplayAction.ALLOWED,
        SitePermissionsRules.AutoplayAction.ALLOWED,
        SitePermissionsRules.Action.ALLOWED,
        SitePermissionsRules.Action.ALLOWED,
        SitePermissionsRules.Action.ALLOWED,
    )
}
