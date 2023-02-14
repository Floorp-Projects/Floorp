/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.content.ActivityNotFoundException
import android.content.Intent
import android.net.Uri
import androidx.test.platform.app.InstrumentationRegistry
import org.mozilla.fenix.BuildConfig.DEEP_LINK_SCHEME

class DeepLinkRobot {
    private fun openDeepLink(url: String) {
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val intent = Intent().apply {
            action = Intent.ACTION_VIEW
            data = Uri.parse("$DEEP_LINK_SCHEME://$url")
            flags = Intent.FLAG_ACTIVITY_NEW_TASK
            addCategory(Intent.CATEGORY_BROWSABLE)
        }
        try {
            context.startActivity(intent)
        } catch (ex: ActivityNotFoundException) {
            intent.setPackage(null)
            context.startActivity(intent)
        }
    }

    fun openURL(url: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
        val deepLink = Uri.parse("open")
            .buildUpon()
            .appendQueryParameter("url", url)
            .build()
            .toString()
        openDeepLink(deepLink)
        return browserScreen(interact)
    }

    fun openHomeScreen(interact: HomeScreenRobot.() -> Unit) =
        openDeepLink("home").run { homeScreen(interact) }

    fun openBookmarks(interact: BookmarksRobot.() -> Unit) =
        openDeepLink("urls_bookmarks").run { bookmarksMenu(interact) }

    fun openHistory(interact: HistoryRobot.() -> Unit) =
        openDeepLink("urls_history").run { historyMenu(interact) }

    fun openCollections(interact: HomeScreenRobot.() -> Unit) =
        openDeepLink("home_collections").run { homeScreen(interact) }

    fun openSettings(interact: SettingsRobot.() -> Unit) =
        openDeepLink("settings").run { settings(interact) }

    fun openSettingsPrivacy(interact: SettingsRobot.() -> Unit) =
        openDeepLink("settings_privacy").run { settings(interact) }

    fun openSettingsLogins(interact: SettingsSubMenuLoginsAndPasswordRobot.() -> Unit) =
        openDeepLink("settings_logins").run { settingsSubMenuLoginsAndPassword(interact) }

    fun openSettingsTrackingProtection(interact: SettingsSubMenuEnhancedTrackingProtectionRobot.() -> Unit) =
        openDeepLink("settings_tracking_protection").run {
            settingsSubMenuEnhancedTrackingProtection(interact)
        }

    fun openSettingsSearchEngine(interact: SettingsSubMenuSearchRobot.() -> Unit) =
        openDeepLink("settings_search_engine").run {
            SettingsSubMenuSearchRobot().interact()
            SettingsSubMenuSearchRobot.Transition()
        }

    fun openSettingsNotification(interact: SystemSettingsRobot.() -> Unit) =
        openDeepLink("settings_notifications").run { systemSettings(interact) }

    fun openMakeDefaultBrowser(interact: SystemSettingsRobot.() -> Unit) =
        openDeepLink("make_default_browser").run { systemSettings(interact) }
}

private fun settings(interact: SettingsRobot.() -> Unit) =
    SettingsRobot().interact().run { SettingsRobot.Transition() }
