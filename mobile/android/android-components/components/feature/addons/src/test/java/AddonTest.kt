/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.webextension.DisabledFlags
import mozilla.components.concept.engine.webextension.Metadata
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
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
            downloadUrl = "downloadUrl",
            version = "version",
            permissions = listOf(
                "bookmarks",
                "browserSettings",
                "browsingData",
                "clipboardRead",
                "clipboardWrite",
                "declarativeNetRequest",
                "declarativeNetRequestFeedback",
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
                "devtools",
            ),
            createdAt = "",
            updatedAt = "",
        )

        val translatedPermissions = addon.translatePermissions(testContext)
        val expectedPermissions = listOf(
            R.string.mozac_feature_addons_permissions_bookmarks_description,
            R.string.mozac_feature_addons_permissions_browser_setting_description,
            R.string.mozac_feature_addons_permissions_browser_data_description,
            R.string.mozac_feature_addons_permissions_clipboard_read_description,
            R.string.mozac_feature_addons_permissions_clipboard_write_description,
            R.string.mozac_feature_addons_permissions_declarative_net_request_description,
            R.string.mozac_feature_addons_permissions_declarative_net_request_feedback_description,
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
            R.string.mozac_feature_addons_permissions_devtools_description,
        ).map { testContext.getString(it) }
        assertEquals(expectedPermissions, translatedPermissions)
    }

    @Test
    fun `isInstalled - true if installed state present and otherwise false`() {
        val addon = Addon(
            id = "id",
            downloadUrl = "downloadUrl",
            version = "version",
            permissions = emptyList(),
            createdAt = "",
            updatedAt = "",
        )
        assertFalse(addon.isInstalled())

        val installedAddon = addon.copy(installedState = Addon.InstalledState("id", "1.0", ""))
        assertTrue(installedAddon.isInstalled())
    }

    @Test
    fun `isEnabled - true if installed state enabled and otherwise false`() {
        val addon = Addon(
            id = "id",
            downloadUrl = "downloadUrl",
            version = "version",
            permissions = emptyList(),
            createdAt = "",
            updatedAt = "",
        )
        assertFalse(addon.isEnabled())

        val installedAddon = addon.copy(installedState = Addon.InstalledState("id", "1.0", ""))
        assertFalse(installedAddon.isEnabled())

        val enabledAddon = addon.copy(installedState = Addon.InstalledState("id", "1.0", "", enabled = true))
        assertTrue(enabledAddon.isEnabled())
    }

    @Test
    fun `filterTranslations - only keeps specified translations`() {
        val addon = Addon(
            id = "id",
            downloadUrl = "downloadUrl",
            version = "version",
            permissions = emptyList(),
            createdAt = "",
            updatedAt = "",
            translatableName = mapOf(Addon.DEFAULT_LOCALE to "name", "de" to "Name", "es" to "nombre"),
            translatableDescription = mapOf(Addon.DEFAULT_LOCALE to "description", "de" to "Beschreibung", "es" to "descripción"),
            translatableSummary = mapOf(Addon.DEFAULT_LOCALE to "summary", "de" to "Kurzfassung", "es" to "resumen"),
        )

        val addonEn = addon.filterTranslations(listOf())
        assertEquals(1, addonEn.translatableName.size)
        assertTrue(addonEn.translatableName.contains(addonEn.defaultLocale))

        val addonEs = addon.filterTranslations(listOf("es"))
        assertEquals(2, addonEs.translatableName.size)
        assertTrue(addonEs.translatableName.contains(addonEn.defaultLocale))
        assertTrue(addonEs.translatableName.contains("es"))
    }

    @Test
    fun `localizedURLAccessPermissions - must translate all_urls access permission`() {
        val expectedString = testContext.getString(R.string.mozac_feature_addons_permissions_all_urls_description)
        val permissions = listOf(
            "webRequest",
            "webRequestBlocking",
            "<all_urls>",
        )

        val result = Addon.localizedURLAccessPermissions(testContext, permissions).first()

        assertEquals(expectedString, result)
    }

    @Test
    fun `localizedURLAccessPermissions - must translate all urls access permission`() {
        val expectedString = testContext.getString(R.string.mozac_feature_addons_permissions_all_urls_description)
        val permissions = listOf(
            "webRequest",
            "webRequestBlocking",
            "*://*/*",
        )

        val result = Addon.localizedURLAccessPermissions(testContext, permissions).first()

        assertEquals(expectedString, result)
    }

    @Test
    fun `localizedURLAccessPermissions - must translate domain access permissions`() {
        val expectedString = listOf("tweetdeck.twitter.com", "twitter.com").map {
            testContext.getString(R.string.mozac_feature_addons_permissions_one_site_description, it)
        }
        testContext.getString(R.string.mozac_feature_addons_permissions_all_urls_description)
        val permissions = listOf(
            "webRequest",
            "webRequestBlocking",
            "*://tweetdeck.twitter.com/*",
            "*://twitter.com/*",
        )

        val result = Addon.localizedURLAccessPermissions(testContext, permissions)

        assertEquals(expectedString, result)
    }

    @Test
    fun `localizedURLAccessPermissions - must translate one site access permissions`() {
        val expectedString = listOf("youtube.com", "youtube-nocookie.com", "vimeo.com").map {
            testContext.getString(R.string.mozac_feature_addons_permissions_sites_in_domain_description, it)
        }
        testContext.getString(R.string.mozac_feature_addons_permissions_all_urls_description)
        val permissions = listOf(
            "webRequest",
            "*://*.youtube.com/*",
            "*://*.youtube-nocookie.com/*",
            "*://*.vimeo.com/*",
        )

        val result = Addon.localizedURLAccessPermissions(testContext, permissions)

        assertEquals(expectedString, result)
    }

    @Test
    fun `localizedURLAccessPermissions - must collapse url access permissions`() {
        var expectedString = listOf("youtube.com", "youtube-nocookie.com", "vimeo.com", "google.co.ao").map {
            testContext.getString(R.string.mozac_feature_addons_permissions_sites_in_domain_description, it)
        } + testContext.getString(R.string.mozac_feature_addons_permissions_one_extra_domain_description)

        var permissions = listOf(
            "webRequest",
            "*://*.youtube.com/*",
            "*://*.youtube-nocookie.com/*",
            "*://*.vimeo.com/*",
            "*://*.google.co.ao/*",
            "*://*.google.com.do/*",
        )

        var result = Addon.localizedURLAccessPermissions(testContext, permissions)

        // 1 domain permissions must be collapsed
        assertEquals(expectedString, result)

        expectedString = listOf("youtube.com", "youtube-nocookie.com", "vimeo.com", "google.co.ao").map {
            testContext.getString(R.string.mozac_feature_addons_permissions_sites_in_domain_description, it)
        } + testContext.getString(R.string.mozac_feature_addons_permissions_extra_domains_description_plural, 2)

        permissions = listOf(
            "webRequest",
            "*://*.youtube.com/*",
            "*://*.youtube-nocookie.com/*",
            "*://*.vimeo.com/*",
            "*://*.google.co.ao/*",
            "*://*.google.com.do/*",
            "*://*.google.co.ar/*",
        )

        result = Addon.localizedURLAccessPermissions(testContext, permissions)

        // 2 domain permissions must be collapsed
        assertEquals(expectedString, result)

        permissions = listOf(
            "webRequest",
            "*://www.youtube.com/*",
            "*://www.youtube-nocookie.com/*",
            "*://www.vimeo.com/*",
            "https://mozilla.org/a/b/c/",
            "*://www.google.com.do/*",
        )

        expectedString = listOf("www.youtube.com", "www.youtube-nocookie.com", "www.vimeo.com", "mozilla.org").map {
            testContext.getString(R.string.mozac_feature_addons_permissions_one_site_description, it)
        } + testContext.getString(R.string.mozac_feature_addons_permissions_one_extra_site_description)

        result = Addon.localizedURLAccessPermissions(testContext, permissions)

        // 1 site permissions must be Collapsed
        assertEquals(expectedString, result)

        permissions = listOf(
            "webRequest",
            "*://www.youtube.com/*",
            "*://www.youtube-nocookie.com/*",
            "*://www.vimeo.com/*",
            "https://mozilla.org/a/b/c/",
            "*://www.google.com.do/*",
            "*://www.google.co.ar/*",
        )

        expectedString = listOf("www.youtube.com", "www.youtube-nocookie.com", "www.vimeo.com", "mozilla.org").map {
            testContext.getString(R.string.mozac_feature_addons_permissions_one_site_description, it)
        } + testContext.getString(R.string.mozac_feature_addons_permissions_extra_sites_description, 2)

        result = Addon.localizedURLAccessPermissions(testContext, permissions)

        // 2 site permissions must be Collapsed
        assertEquals(expectedString, result)

        permissions = listOf(
            "webRequest",
            "*://www.youtube.com/*",
            "*://www.youtube-nocookie.com/*",
            "*://www.vimeo.com/*",
            "https://mozilla.org/a/b/c/",
        )

        expectedString = listOf("www.youtube.com", "www.youtube-nocookie.com", "www.vimeo.com", "mozilla.org").map {
            testContext.getString(R.string.mozac_feature_addons_permissions_one_site_description, it)
        }

        result = Addon.localizedURLAccessPermissions(testContext, permissions)

        // None permissions must be collapsed
        assertEquals(expectedString, result)
    }

    @Test
    fun `localizeURLAccessPermission - must provide the correct localized string`() {
        val siteAccess = listOf(
            "*://twitter.com/*",
            "*://tweetdeck.twitter.com/*",
            "https://mozilla.org/a/b/c/",
            "https://www.google.com.ag/*",
            "https://www.google.co.ck/*",
        )

        siteAccess.forEach {
            val stringId = Addon.localizeURLAccessPermission(it)
            assertEquals(R.string.mozac_feature_addons_permissions_one_site_description, stringId)
        }

        val domainAccess = listOf(
            "*://*.youtube.com/*",
            "*://*.youtube.com/*",
            "*://*.youtube-nocookie.com/*",
            "*://*.vimeo.com/*",
            "*://*.facebookcorewwwi.onion/*",
        )

        domainAccess.forEach {
            val stringId = Addon.localizeURLAccessPermission(it)
            assertEquals(R.string.mozac_feature_addons_permissions_sites_in_domain_description, stringId)
        }

        val allUrlsAccess = listOf(
            "*://*/*",
            "http://*/*",
            "https://*/*",
            "file://*/*",
            "<all_urls>",
        )

        allUrlsAccess.forEach {
            val stringId = Addon.localizeURLAccessPermission(it)
            assertEquals(R.string.mozac_feature_addons_permissions_all_urls_description, stringId)
        }
    }

    @Test
    fun `newFromWebExtension - must return an Addon instance`() {
        val version = "1.2.3"
        val permissions = listOf("scripting", "activeTab")
        val hostPermissions = listOf("https://example.org/")
        val name = "some name"
        val description = "some description"
        val extension: WebExtension = mock()
        val metadata: Metadata = mock()
        whenever(extension.id).thenReturn("some-id")
        whenever(extension.url).thenReturn("some-url")
        whenever(extension.getMetadata()).thenReturn(metadata)
        whenever(metadata.version).thenReturn(version)
        whenever(metadata.permissions).thenReturn(permissions)
        whenever(metadata.hostPermissions).thenReturn(hostPermissions)
        whenever(metadata.name).thenReturn(name)
        whenever(metadata.description).thenReturn(description)
        whenever(metadata.disabledFlags).thenReturn(DisabledFlags.select(0))
        whenever(metadata.baseUrl).thenReturn("some-base-url")
        whenever(metadata.developerName).thenReturn("developer-name")
        whenever(metadata.developerUrl).thenReturn("developer-url")
        whenever(metadata.fullDescription).thenReturn("fullDescription")
        whenever(metadata.homePageUrl).thenReturn("some-url")
        whenever(metadata.downloadUrl).thenReturn("some-download-url")
        whenever(metadata.updateDate).thenReturn("1970-01-01T00:00:00Z")
        whenever(metadata.reviewUrl).thenReturn("some-review-url")
        whenever(metadata.reviewCount).thenReturn(0)
        whenever(metadata.averageRating).thenReturn(0f)
        val addon = Addon.newFromWebExtension(extension)

        assertEquals("some-id", addon.id)
        assertEquals("some-url", addon.siteUrl)
        assertEquals("some-download-url", addon.downloadUrl)
        assertEquals(permissions + hostPermissions, addon.permissions)
        assertEquals("", addon.updatedAt)
        assertEquals("some name", addon.translatableName[Addon.DEFAULT_LOCALE])
        assertEquals("fullDescription", addon.translatableDescription[Addon.DEFAULT_LOCALE])
        assertEquals("some description", addon.translatableSummary[Addon.DEFAULT_LOCALE])
        assertEquals("developer-name", addon.author?.name)
        assertEquals("developer-url", addon.author?.url)
        assertEquals("some-download-url", addon.downloadUrl)
        assertEquals("some-review-url", addon.ratingUrl)
        assertEquals(0, addon.rating!!.reviews)
    }

    @Test
    fun `fromMetadataToAddonDate - must return an valid addon formatted date`() {
        val expectedDate = "2023-09-28T00:37:43Z"
        val inputDate = "2023-09-28T00:37:43.983Z"

        val result = Addon.fromMetadataToAddonDate(inputDate)

        assertEquals(expectedDate, result)
    }

    @Test
    fun `fromMetadataToAddonDate - must return  handle invalid date formats`() {
        val expectedDate = ""
        val inputDate = "202xd3-09-28T00:37:43.98993Z"

        val result = Addon.fromMetadataToAddonDate(inputDate)

        assertEquals(expectedDate, result)
    }

    @Test
    fun `fromMetadataToAddonDate - must return empty strings`() {
        val expectedDate = ""
        val inputDate = ""

        val result = Addon.fromMetadataToAddonDate(inputDate)

        assertEquals(expectedDate, result)
    }

    @Test
    fun `isDisabledAsBlocklisted - true if installed state disabled status equals to BLOCKLISTED and otherwise false`() {
        val addon = Addon(id = "id")
        val blockListedAddon = addon.copy(
            installedState = Addon.InstalledState(
                id = "id",
                version = "1.0",
                optionsPageUrl = "",
                disabledReason = Addon.DisabledReason.BLOCKLISTED,
            ),
        )

        assertFalse(addon.isDisabledAsBlocklisted())
        assertTrue(blockListedAddon.isDisabledAsBlocklisted())
    }

    @Test
    fun `isDisabledAsNotCorrectlySigned - true if installed state disabled status equals to NOT_CORRECTLY_SIGNED and otherwise false`() {
        val addon = Addon(id = "id")
        val blockListedAddon = addon.copy(
            installedState = Addon.InstalledState(
                id = "id",
                version = "1.0",
                optionsPageUrl = "",
                disabledReason = Addon.DisabledReason.NOT_CORRECTLY_SIGNED,
            ),
        )

        assertFalse(addon.isDisabledAsNotCorrectlySigned())
        assertTrue(blockListedAddon.isDisabledAsNotCorrectlySigned())
    }

    @Test
    fun `isDisabledAsIncompatible - true if installed state disabled status equals to INCOMPATIBLE and otherwise false`() {
        val addon = Addon(id = "id")
        val blockListedAddon = addon.copy(
            installedState = Addon.InstalledState(
                id = "id",
                version = "1.0",
                optionsPageUrl = "",
                disabledReason = Addon.DisabledReason.INCOMPATIBLE,
            ),
        )

        assertFalse(addon.isDisabledAsIncompatible())
        assertTrue(blockListedAddon.isDisabledAsIncompatible())
    }
}
