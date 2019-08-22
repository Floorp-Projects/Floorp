/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.intent

import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import android.provider.Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS
import androidx.navigation.NavController
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.browser.browsingmode.BrowsingMode

/**
 * Deep links in the form of `fenix://host` open different parts of the app.
 */
class DeepLinkIntentProcessor(
    private val activity: HomeActivity
) : HomeIntentProcessor {

    override fun process(intent: Intent, navController: NavController, out: Intent): Boolean {
        return if (intent.scheme == "fenix") {
            intent.data?.let { handleDeepLink(it, navController) }
            true
        } else {
            false
        }
    }

    private fun handleDeepLink(deepLink: Uri, navController: NavController) {
        handleDeepLinkSideEffects(deepLink)

        val directions = when (deepLink.host) {
            "home", "enable_private_browsing" -> NavGraphDirections.actionGlobalHomeFragment()
            "settings" -> NavGraphDirections.actionGlobalSettingsFragment()
            "turn_on_sync" -> NavGraphDirections.actionGlobalTurnOnSync()
            "settings_search_engine" -> NavGraphDirections.actionGlobalSearchEngineFragment()
            "settings_accessibility" -> NavGraphDirections.actionGlobalAccessibilityFragment()
            "settings_delete_browsing_data" -> NavGraphDirections.actionGlobalDeleteBrowsingDataFragment()
            else -> return
        }

        navController.navigate(directions)
    }

    /**
     * Handle links that require more than just simple navigation.
     */
    private fun handleDeepLinkSideEffects(deepLink: Uri) {
        when (deepLink.host) {
            "enable_private_browsing" -> {
                activity.browsingModeManager.mode = BrowsingMode.Private
            }
            "make_default_browser" -> {
                if (SDK_INT >= Build.VERSION_CODES.N) {
                    val settingsIntent = Intent(ACTION_MANAGE_DEFAULT_APPS_SETTINGS)
                    activity.startActivity(settingsIntent)
                }
            }
            "open" -> {
                deepLink.getQueryParameter("url")?.let { searchTermOrUrl ->
                    activity.openToBrowserAndLoad(
                        searchTermOrUrl,
                        newTab = true,
                        from = BrowserDirection.FromGlobal
                    )
                }
            }
        }
    }
}
