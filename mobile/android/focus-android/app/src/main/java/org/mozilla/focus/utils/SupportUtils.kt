/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils

import android.annotation.TargetApi
import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.provider.Settings
import mozilla.components.browser.state.state.SessionState
import org.mozilla.focus.ext.components
import org.mozilla.focus.locale.Locales
import org.mozilla.focus.state.AppAction
import java.io.UnsupportedEncodingException
import java.net.URLEncoder
import java.util.Locale

object SupportUtils {
    const val HELP_URL = "https://support.mozilla.org/kb/what-firefox-focus-android"
    const val DEFAULT_BROWSER_URL = "https://support.mozilla.org/kb/set-firefox-focus-default-browser-android"
    const val REPORT_SITE_ISSUE_URL = "https://webcompat.com/issues/new?url=%s&label=browser-focus-geckoview"
    const val PRIVACY_NOTICE_URL = "https://www.mozilla.org/privacy/firefox-focus/"
    const val PRIVACY_NOTICE_KLAR_URL = "https://www.mozilla.org/de/privacy/firefox-klar/"

    const val OPEN_WITH_DEFAULT_BROWSER_URL = "https://www.mozilla.org/openGeneralSettings" // Fake URL
    val manifestoURL: String
        get() {
            val langTag = Locales.getLanguageTag(Locale.getDefault())
            return "https://www.mozilla.org/$langTag/about/manifesto/"
        }

    enum class SumoTopic(
        /** The final path segment for a SUMO URL - see {@see #getSumoURLForTopic}  */
        internal val topicStr: String
    ) {
        ADD_SEARCH_ENGINE("add-search-engine"),
        AUTOCOMPLETE("autofill-domain-android"),
        TRACKERS("trackers"),
        USAGE_DATA("usage-data"),
        WHATS_NEW("whats-new-focus-android-8"),
        SEARCH_SUGGESTIONS("search-suggestions-focus-android"),
        ALLOWLIST("focus-android-allowlist")
    }

    fun getSumoURLForTopic(context: Context, topic: SumoTopic): String {
        val escapedTopic = getEncodedTopicUTF8(topic.topicStr)
        val appVersion = getAppVersion(context)
        val osTarget = "Android"
        val langTag = Locales.getLanguageTag(Locale.getDefault())
        return "https://support.mozilla.org/1/mobile/$appVersion/$osTarget/$langTag/$escapedTopic"
    }

    // For some reason this URL has a different format than the other SUMO URLs
    fun getSafeBrowsingURL(): String {
        val langTag = Locales.getLanguageTag(Locale.getDefault())
        return "https://support.mozilla.org/$langTag/kb/how-does-phishing-and-malware-protection-work"
    }

    private fun getEncodedTopicUTF8(topic: String): String {
        try {
            return URLEncoder.encode(topic, "UTF-8")
        } catch (e: UnsupportedEncodingException) {
            throw IllegalStateException("utf-8 should always be available", e)
        }
    }

    private fun getAppVersion(context: Context): String {
        try {
            return context.packageManager.getPackageInfo(context.packageName, 0).versionName
        } catch (e: PackageManager.NameNotFoundException) {
            // This should be impossible - we should always be able to get information about ourselves:
            throw IllegalStateException("Unable find package details for Focus", e)
        }
    }

    fun openDefaultBrowserSumoPage(context: Context) {
        val tabId = context.components.tabsUseCases.addTab(
            DEFAULT_BROWSER_URL,
            source = SessionState.Source.Internal.Menu,
            selectTab = true,
            private = true
        )

        context.components.appStore.dispatch(
            AppAction.OpenTab(tabId)
        )
    }

    @TargetApi(Build.VERSION_CODES.N)
    fun openDefaultAppsSettings(context: Context) {
        try {
            val intent = Intent(Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS)
            context.startActivity(intent)
        } catch (e: ActivityNotFoundException) {
            // In some cases, a matching Activity may not exist (according to the Android docs).
            openDefaultBrowserSumoPage(context)
        }
    }
}
