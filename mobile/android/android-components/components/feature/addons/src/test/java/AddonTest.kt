/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.amo

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.R
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class AddonTest {

    @Test
    fun `translatePermissions - must return the expected string ids per permission category`() {
        val addon = Addon(
            id = "id",
            authors = emptyList(),
            categories = emptyList(),
            downloadUrl = "downloadUrl",
            version = "version",
            permissions = listOf(
                "bookmarks",
                "browserSettings",
                "browsingData",
                "clipboardRead",
                "clipboardWrite",
                "downloads",
                "downloads.open",
                "find",
                "geolocation",
                "history",
                "management",
                "nativeMessaging",
                "notifications",
                "pkcs11",
                "privacy",
                "proxy",
                "sessions",
                "tabHide",
                "tabs",
                "topSites",
                "unlimitedStorage",
                "webNavigation",
                "devtools"
            ),
            createdAt = "",
            updatedAt = ""
        )

        val translatedPermissions = addon.translatePermissions()
        val expectedPermissions = listOf(
            R.string.mozac_feature_addons_permissions_bookmarks_description,
            R.string.mozac_feature_addons_permissions_browser_setting_description,
            R.string.mozac_feature_addons_permissions_browser_data_description,
            R.string.mozac_feature_addons_permissions_clipboard_read_description,
            R.string.mozac_feature_addons_permissions_clipboard_write_description,
            R.string.mozac_feature_addons_permissions_downloads_description,
            R.string.mozac_feature_addons_permissions_downloads_open_description,
            R.string.mozac_feature_addons_permissions_find_description,
            R.string.mozac_feature_addons_permissions_geolocation_description,
            R.string.mozac_feature_addons_permissions_history_description,
            R.string.mozac_feature_addons_permissions_management_description,
            R.string.mozac_feature_addons_permissions_native_messaging_description,
            R.string.mozac_feature_addons_permissions_notifications_description,
            R.string.mozac_feature_addons_permissions_pkcs11_description,
            R.string.mozac_feature_addons_permissions_privacy_description,
            R.string.mozac_feature_addons_permissions_proxy_description,
            R.string.mozac_feature_addons_permissions_sessions_description,
            R.string.mozac_feature_addons_permissions_tab_hide_description,
            R.string.mozac_feature_addons_permissions_tabs_description,
            R.string.mozac_feature_addons_permissions_top_sites_description,
            R.string.mozac_feature_addons_permissions_unlimited_storage_description,
            R.string.mozac_feature_addons_permissions_web_navigation_description,
            R.string.mozac_feature_addons_permissions_top_sites_description
        )
        assertEquals(expectedPermissions, translatedPermissions)
    }

    @Test
    fun `isInstalled - true if installed state present and otherwise false`() {
        val addon = Addon(
            id = "id",
            authors = emptyList(),
            categories = emptyList(),
            downloadUrl = "downloadUrl",
            version = "version",
            permissions = emptyList(),
            createdAt = "",
            updatedAt = ""
        )
        assertFalse(addon.isInstalled())

        val installedAddon = addon.copy(installedState = Addon.InstalledState("id", "1.0", ""))
        assertTrue(installedAddon.isInstalled())
    }

    @Test
    fun `isEnabled - true if installed state enabled and otherwise false`() {
        val addon = Addon(
            id = "id",
            authors = emptyList(),
            categories = emptyList(),
            downloadUrl = "downloadUrl",
            version = "version",
            permissions = emptyList(),
            createdAt = "",
            updatedAt = ""

        )
        assertFalse(addon.isEnabled())

        val installedAddon = addon.copy(installedState = Addon.InstalledState("id", "1.0", ""))
        assertFalse(installedAddon.isEnabled())

        val enabledAddon = addon.copy(installedState = Addon.InstalledState("id", "1.0", "", enabled = true))
        assertTrue(enabledAddon.isEnabled())
    }
}