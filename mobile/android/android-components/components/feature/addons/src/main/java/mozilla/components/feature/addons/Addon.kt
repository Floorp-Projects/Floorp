/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Bitmap
import android.os.Parcelable
import androidx.core.net.toUri
import kotlinx.parcelize.Parcelize

/**
 * Represents an add-on based on the AMO store:
 * https://addons.mozilla.org/en-US/firefox/
 *
 * @property id The unique ID of this add-on.
 * @property authors List holding information about the add-on authors.
 * @property categories List of categories the add-on belongs to.
 * @property downloadId The unique ID of the latest version of the add-on (xpi) file.
 * @property downloadUrl The (absolute) URL to download the latest version of the add-on file.
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
 * @property defaultLocale Indicates which locale will be always available to display translatable fields.
 */
@SuppressLint("ParcelCreator")
@Parcelize
data class Addon(
    val id: String,
    val authors: List<Author> = emptyList(),
    val categories: List<String> = emptyList(),
    val downloadId: String = "",
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
    val installedState: InstalledState? = null,
    val defaultLocale: String = DEFAULT_LOCALE,
) : Parcelable {
    /**
     * Represents an add-on author.
     *
     * @property id The id of the author (creator) of the add-on.
     * @property name The name of the author.
     * @property url The link to the profile page for of the author.
     * @property username The username of the author.
     */
    @SuppressLint("ParcelCreator")
    @Parcelize
    data class Author(
        val id: String,
        val name: String,
        val url: String,
        val username: String,
    ) : Parcelable

    /**
     * Holds all the rating information of this add-on.
     *
     * @property average An average score from 1 to 5 of how users scored this add-on.
     * @property reviews The number of users that has scored this add-on.
     */
    @SuppressLint("ParcelCreator")
    @Parcelize
    data class Rating(
        val average: Float,
        val reviews: Int,
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
     * @property allowedInPrivateBrowsing true if this addon should be allowed to run in private
     * browsing pages, false otherwise.
     * @property icon the icon of the installed extension, only used for temporary extensions
     * as we get the icon from AMO otherwise, see [iconUrl].
     */
    @SuppressLint("ParcelCreator")
    @Parcelize
    data class InstalledState(
        val id: String,
        val version: String,
        val optionsPageUrl: String?,
        val openOptionsPageInTab: Boolean = false,
        val enabled: Boolean = false,
        val supported: Boolean = true,
        val disabledAsUnsupported: Boolean = false,
        val allowedInPrivateBrowsing: Boolean = false,
        val icon: Bitmap? = null,
    ) : Parcelable

    /**
     * Returns a list of localized Strings per each item on the [permissions] list.
     * @param context A context reference.
     */
    fun translatePermissions(context: Context): List<String> {
        return localizePermissions(permissions, context)
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
     * Returns whether or not this [Addon] is allowed in private browsing mode.
     */
    fun isAllowedInPrivateBrowsing() = installedState?.allowedInPrivateBrowsing == true

    /**
     * Returns a copy of this [Addon] containing only translations (description,
     * name, summary) of the provided locales. All other translations
     * except the [defaultLocale] will be removed.
     *
     * @param locales list of locales to keep.
     * @return copy of the addon with all other translations removed.
     */
    fun filterTranslations(locales: List<String>): Addon {
        val internalLocales = locales + defaultLocale
        val descriptions = translatableDescription.filterKeys { internalLocales.contains(it) }
        val names = translatableName.filterKeys { internalLocales.contains(it) }
        val summaries = translatableSummary.filterKeys { internalLocales.contains(it) }
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
            "declarativeNetRequest" to R.string.mozac_feature_addons_permissions_declarative_net_request_description,
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
            "devtools" to R.string.mozac_feature_addons_permissions_devtools_description,
        )

        /**
         * Takes a list of [permissions] and returns a list of id resources per each item.
         * @param permissions The list of permissions to be localized. Valid permissions can be found in
         * https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/permissions#API_permissions
         */
        fun localizePermissions(permissions: List<String>, context: Context): List<String> {
            var localizedUrlAccessPermissions = emptyList<String>()
            val requireAllUrlsAccess = permissions.contains("<all_urls>")
            val notFoundPermissions = mutableListOf<String>()

            val localizedNormalPermissions = permissions.mapNotNull {
                val id = permissionToTranslation[it]
                if (id == null) notFoundPermissions.add(it)
                id
            }.map { context.getString(it) }

            if (!requireAllUrlsAccess && notFoundPermissions.isNotEmpty()) {
                localizedUrlAccessPermissions = localizedURLAccessPermissions(context, notFoundPermissions)
            }

            return localizedNormalPermissions + localizedUrlAccessPermissions
        }

        @Suppress("MaxLineLength")
        internal fun localizedURLAccessPermissions(context: Context, accessPermissions: List<String>): List<String> {
            val localizedSiteAccessPermissions = mutableListOf<String>()
            val permissionsToTranslations = mutableMapOf<String, Int>()

            accessPermissions.forEach { permission ->
                val id = localizeURLAccessPermission(permission)
                if (id != null) {
                    permissionsToTranslations[permission] = id
                }
            }

            if (permissionsToTranslations.values.any { it.isAllURLsPermission() }) {
                localizedSiteAccessPermissions.add(context.getString(R.string.mozac_feature_addons_permissions_all_urls_description))
            } else {
                formatURLAccessPermission(permissionsToTranslations, localizedSiteAccessPermissions, context)
            }
            return localizedSiteAccessPermissions
        }

        @Suppress("MagicNumber", "ComplexMethod")
        private fun formatURLAccessPermission(
            permissionsToTranslations: MutableMap<String, Int>,
            localizedSiteAccessPermissions: MutableList<String>,
            context: Context,
        ) {
            val maxShownPermissionsEntries = 4
            fun addExtraEntriesIfNeeded(count: Int, oneExtraPermission: Int, multiplePermissions: Int) {
                val collapsedPermissions = count - maxShownPermissionsEntries
                if (collapsedPermissions == 1) {
                    localizedSiteAccessPermissions.add(context.getString(oneExtraPermission))
                } else {
                    localizedSiteAccessPermissions.add(context.getString(multiplePermissions, collapsedPermissions))
                }
            }

            var domainCount = 0
            var siteCount = 0

            loop@ for ((permission, stringId) in permissionsToTranslations) {
                var host = permission.toUri().host ?: ""
                when {
                    stringId.isDomainAccessPermission() -> {
                        ++domainCount
                        host = host.removePrefix("*.")

                        if (domainCount > maxShownPermissionsEntries) continue@loop
                    }

                    stringId.isSiteAccessPermission() -> {
                        ++siteCount
                        if (siteCount > maxShownPermissionsEntries) continue@loop
                    }
                }
                localizedSiteAccessPermissions.add(context.getString(stringId, host))
            }

            // If we have [maxPermissionsEntries] or fewer permissions, display them all, otherwise we
            // display the first [maxPermissionsEntries] followed by an item that says "...plus N others"
            if (domainCount > maxShownPermissionsEntries) {
                val onePermission = R.string.mozac_feature_addons_permissions_one_extra_domain_description
                val multiplePermissions = R.string.mozac_feature_addons_permissions_extra_domains_description_plural
                addExtraEntriesIfNeeded(domainCount, onePermission, multiplePermissions)
            }
            if (siteCount > maxShownPermissionsEntries) {
                val onePermission = R.string.mozac_feature_addons_permissions_one_extra_site_description
                val multiplePermissions = R.string.mozac_feature_addons_permissions_extra_sites_description
                addExtraEntriesIfNeeded(siteCount, onePermission, multiplePermissions)
            }
        }

        private fun Int.isSiteAccessPermission(): Boolean {
            return this == R.string.mozac_feature_addons_permissions_one_site_description
        }

        private fun Int.isDomainAccessPermission(): Boolean {
            return this == R.string.mozac_feature_addons_permissions_sites_in_domain_description
        }

        private fun Int.isAllURLsPermission(): Boolean {
            return this == R.string.mozac_feature_addons_permissions_all_urls_description
        }

        internal fun localizeURLAccessPermission(urlAccess: String): Int? {
            val uri = urlAccess.toUri()
            val host = (uri.host ?: "").trim()
            val path = (uri.path ?: "").trim()

            return when {
                host == "*" || urlAccess == "<all_urls>" -> {
                    R.string.mozac_feature_addons_permissions_all_urls_description
                }
                host.isEmpty() || path.isEmpty() -> null
                host.startsWith(prefix = "*.") -> R.string.mozac_feature_addons_permissions_sites_in_domain_description
                else -> R.string.mozac_feature_addons_permissions_one_site_description
            }
        }

        /**
         * The default fallback locale in case the [Addon] does not have its own [Addon.defaultLocale].
         */
        const val DEFAULT_LOCALE = "en-us"
    }
}
