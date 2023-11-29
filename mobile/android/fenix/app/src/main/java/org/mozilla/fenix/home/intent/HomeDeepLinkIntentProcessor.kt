/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.intent

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import android.provider.Settings
import androidx.navigation.NavController
import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.GlobalDirections
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.ext.alreadyOnDestination
import org.mozilla.fenix.ext.openSetDefaultBrowserOption

/**
 * Deep links in the form of `fenix://host` open different parts of the app.
 */
class HomeDeepLinkIntentProcessor(
    private val activity: HomeActivity,
    private val appStore: AppStore,
) : HomeIntentProcessor {
    private val logger = Logger("DeepLinkIntentProcessor")

    override fun process(intent: Intent, navController: NavController, out: Intent): Boolean {
        val scheme = intent.scheme?.equals(BuildConfig.DEEP_LINK_SCHEME, ignoreCase = true) ?: return false
        return if (scheme) {
            intent.data?.let { handleDeepLink(it, navController) }
            true
        } else {
            false
        }
    }

    @Suppress("ComplexMethod")
    private fun handleDeepLink(deepLink: Uri, navController: NavController) {
        handleDeepLinkSideEffects(deepLink)

        val globalDirections = when (deepLink.host) {
            "home", "enable_private_browsing" -> GlobalDirections.Home
            "urls_bookmarks" -> GlobalDirections.Bookmarks
            "urls_history" -> GlobalDirections.History
            "settings" -> GlobalDirections.Settings
            "turn_on_sync" -> GlobalDirections.Sync
            "settings_search_engine" -> GlobalDirections.SearchEngine
            "settings_accessibility" -> GlobalDirections.Accessibility
            "settings_delete_browsing_data" -> GlobalDirections.DeleteData
            "settings_addon_manager" -> GlobalDirections.SettingsAddonManager
            "settings_logins" -> GlobalDirections.SettingsLogins
            "settings_tracking_protection" -> GlobalDirections.SettingsTrackingProtection
            // We'd like to highlight views within the fragment
            // https://github.com/mozilla-mobile/fenix/issues/11856
            // The current version of UI has these features in more complex screens.
            "settings_privacy" -> GlobalDirections.Settings
            "settings_wallpapers" -> GlobalDirections.WallpaperSettings
            "home_collections" -> GlobalDirections.Home

            else -> return
        }

        if (!navController.alreadyOnDestination(globalDirections.destinationId)) {
            navController.navigate(globalDirections.navDirections)
        }
    }

    /**
     * Handle links that require more than just simple navigation.
     */
    private fun handleDeepLinkSideEffects(deepLink: Uri) {
        when (deepLink.host) {
            "enable_private_browsing" -> {
                appStore.dispatch(AppAction.IntentAction.EnterPrivateBrowsing)
            }
            "make_default_browser" -> {
                activity.openSetDefaultBrowserOption(
                    from = BrowserDirection.FromGlobal,
                    flags = EngineSession.LoadUrlFlags.external(),
                )
            }
            "open" -> {
                val url = deepLink.getQueryParameter("url")
                if (url == null || !url.startsWith("https://")) {
                    logger.info("Not opening deep link: $url")
                    return
                }

                activity.openToBrowserAndLoad(
                    url,
                    newTab = true,
                    from = BrowserDirection.FromGlobal,
                    flags = EngineSession.LoadUrlFlags.external(),
                )
            }
            "settings_notifications" -> {
                val intent = notificationSettings(activity)
                activity.startActivity(intent)
            }
        }
    }

    private fun notificationSettings(context: Context, channel: String? = null) =
        Intent().apply {
            when {
                SDK_INT >= Build.VERSION_CODES.O -> {
                    action = channel?.let {
                        putExtra(Settings.EXTRA_CHANNEL_ID, it)
                        Settings.ACTION_CHANNEL_NOTIFICATION_SETTINGS
                    } ?: Settings.ACTION_APP_NOTIFICATION_SETTINGS
                    putExtra(Settings.EXTRA_APP_PACKAGE, context.packageName)
                }
                else -> {
                    action = "android.settings.APP_NOTIFICATION_SETTINGS"
                    putExtra("app_package", context.packageName)
                    putExtra("app_uid", context.applicationInfo.uid)
                }
            }
        }
}
