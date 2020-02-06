/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons

import android.os.Parcelable
import kotlinx.android.parcel.Parcelize

/**
 * Represents an add-on based on the AMO store:
 * https://addons.mozilla.org/en-US/firefox/
 *
 * @property id The unique ID of this add-on.
 * @property authors List holding information about the add-on authors.
 * @property categories List of categories the add-on belongs to.
 * @property downloadUrl The (absolute) URL to download the add-on file (eg xpi).
 * @property version The add-on version e.g "1.23.0".
 * @property permissions List of the add-on permissions for this File.
 * @property translatableName A map containing the different translations for the add-on name,
 * where the key is the language and the value is the actual translated text.
 * @property translatableDescription A map containing the different translations for the add-on description,
 * where the key is the language and the value is the actual translated text.
 * @property translatableSummary A map containing the different translations for the add-on name,
 * where the key is the language and the value is the actual translated text.
 * @property iconUrl The URL to icon for the add-on.
 * @property siteUrl The (absolute) add-on detail URL.
 * @property rating The rating information of this add-on.
 * @property createdAt The date the add-on was created.
 * @property updatedAt The date of the last time the add-on was updated by its developer(s).
 * @property installedState Holds the state of the installed web extension for this add-on. Null, if
 * the [Addon] is not installed.
 */
@Parcelize
data class Addon(
    val id: String,
    val authors: List<Author> = emptyList(),
    val categories: List<String> = emptyList(),
    val downloadUrl: String = "",
    val version: String = "",
    val permissions: List<String> = emptyList(),
    val translatableName: Map<String, String> = emptyMap(),
    val translatableDescription: Map<String, String> = emptyMap(),
    val translatableSummary: Map<String, String> = emptyMap(),
    val iconUrl: String = "",
    val siteUrl: String = "",
    val rating: Rating? = null,
    val createdAt: String = "",
    val updatedAt: String = "",
    val installedState: InstalledState? = null
) : Parcelable {
    /**
     * Represents an add-on author.
     *
     * @property id The id of the author (creator) of the add-on.
     * @property name The name of the author.
     * @property url The link to the profile page for of the author.
     * @property username The username of the author.
     */
    @Parcelize
    data class Author(
        val id: String,
        val name: String,
        val url: String,
        val username: String
    ) : Parcelable

    /**
     * Holds all the rating information of this add-on.
     *
     * @property average An average score from 1 to 5 of how users scored this add-on.
     * @property reviews The number of users that has scored this add-on.
     */
    @Parcelize
    data class Rating(
        val average: Float,
        val reviews: Int
    ) : Parcelable

    /**
     * Returns a list of id resources per each item on the [Addon.permissions] list.
     * Holds the state of the installed web extension of this add-on.
     *
     * @property id The ID of the installed web extension.
     * @property version The installed version.
     * @property enabled Indicates if this [Addon] is enabled to interact with web content or not,
     * defaults to false.
     * @property supported Indicates if this [Addon] is supported by the browser or not, defaults
     * to true.
     * @property optionsPageUrl the URL of the page displaying the
     * options page (options_ui in the extension's manifest).
     */
    @Parcelize
    data class InstalledState(
        val id: String,
        val version: String,
        val optionsPageUrl: String,
        val enabled: Boolean = false,
        val supported: Boolean = true,
        val disabledAsUnsupported: Boolean = false
    ) : Parcelable

    /**
     * Returns a list of id resources per each item on the [permissions] list.
     */
    fun translatePermissions(): List<Int> {
        return localizePermissions(permissions)
    }

    /**
     * Returns whether or not this [Addon] is currently installed.
     */
    fun isInstalled() = installedState != null

    /**
     * Returns whether or not this [Addon] is currently enabled.
     */
    fun isEnabled() = installedState?.enabled == true

    /**
     * Returns whether or not this [Addon] is currently supported by the browser.
     */
    fun isSupported() = installedState?.supported == true

    /**
     * Returns whether or not this [Addon] is currently disabled because it is not
     * supported. This is based on the installed extension state in the engine. An
     * addon can be disabled as unsupported and later become supported, so
     * both [isSupported] and [isDisabledAsUnsupported] can be true.
     */
    fun isDisabledAsUnsupported() = installedState?.disabledAsUnsupported == true

    /**
     * Returns a copy of this [Addon] containing only translations (description,
     * name, summary) of the provided locales. All other translations
     * will be removed.
     *
     * @param locales list of locales to keep.
     * @return copy of the addon with all other translations removed.
     */
    fun filterTranslations(locales: List<String>): Addon {
        val descriptions = translatableDescription.filterKeys { locales.contains(it) }
        val names = translatableName.filterKeys { locales.contains(it) }
        val summaries = translatableSummary.filterKeys { locales.contains(it) }
        return copy(translatableName = names, translatableDescription = descriptions, translatableSummary = summaries)
    }

    companion object {
        /**
         * A map of permissions to translation string ids.
         */
        val permissionToTranslation = mapOf(
            "privacy" to R.string.mozac_feature_addons_permissions_privacy_description,
            "<all_urls>" to R.string.mozac_feature_addons_permissions_all_urls_description,
            "tabs" to R.string.mozac_feature_addons_permissions_tabs_description,
            "unlimitedStorage" to R.string.mozac_feature_addons_permissions_unlimited_storage_description,
            "webNavigation" to R.string.mozac_feature_addons_permissions_web_navigation_description,
            "bookmarks" to R.string.mozac_feature_addons_permissions_bookmarks_description,
            "browserSettings" to R.string.mozac_feature_addons_permissions_browser_setting_description,
            "browsingData" to R.string.mozac_feature_addons_permissions_browser_data_description,
            "clipboardRead" to R.string.mozac_feature_addons_permissions_clipboard_read_description,
            "clipboardWrite" to R.string.mozac_feature_addons_permissions_clipboard_write_description,
            "downloads" to R.string.mozac_feature_addons_permissions_downloads_description,
            "downloads.open" to R.string.mozac_feature_addons_permissions_downloads_open_description,
            "find" to R.string.mozac_feature_addons_permissions_find_description,
            "geolocation" to R.string.mozac_feature_addons_permissions_geolocation_description,
            "history" to R.string.mozac_feature_addons_permissions_history_description,
            "management" to R.string.mozac_feature_addons_permissions_management_description,
            "nativeMessaging" to R.string.mozac_feature_addons_permissions_native_messaging_description,
            "notifications" to R.string.mozac_feature_addons_permissions_notifications_description,
            "pkcs11" to R.string.mozac_feature_addons_permissions_pkcs11_description,
            "proxy" to R.string.mozac_feature_addons_permissions_proxy_description,
            "sessions" to R.string.mozac_feature_addons_permissions_sessions_description,
            "tabHide" to R.string.mozac_feature_addons_permissions_tab_hide_description,
            "topSites" to R.string.mozac_feature_addons_permissions_top_sites_description,
            "devtools" to R.string.mozac_feature_addons_permissions_top_sites_description
        )

        /**
         * Takes a list of [permissions] and returns a list of id resources per each item.
         * @param permissions The list of permissions to be localized. Valid permissions can be found in
         * https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/permissions#API_permissions
         */
        fun localizePermissions(permissions: List<String>): List<Int> {
            return permissions.mapNotNull { permissionToTranslation[it] }
        }
    }
}
