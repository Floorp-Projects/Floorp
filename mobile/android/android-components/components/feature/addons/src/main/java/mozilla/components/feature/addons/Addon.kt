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
 * @property installed Indicates if this [Addon] is installed or not, defaults to false.
 * @property enabled Indicates if this [Addon] is enabled to interact with web content or not,
 * defaults to false.
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
 */
@Parcelize
data class Addon(
    val id: String,
    val authors: List<Author>,
    val categories: List<String>,
    val installed: Boolean = false,
    val enabled: Boolean = false,
    val downloadUrl: String,
    val version: String,
    val permissions: List<String> = emptyList(),
    val translatableName: Map<String, String> = emptyMap(),
    val translatableDescription: Map<String, String> = emptyMap(),
    val translatableSummary: Map<String, String> = emptyMap(),
    val iconUrl: String = "",
    val siteUrl: String = "",
    val rating: Rating? = null,
    val createdAt: String,
    val updatedAt: String
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
     */
    fun translatePermissions(): List<Int> {
        return permissions.mapNotNull { it -> permissionToTranslation[it] }
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
    }
}
