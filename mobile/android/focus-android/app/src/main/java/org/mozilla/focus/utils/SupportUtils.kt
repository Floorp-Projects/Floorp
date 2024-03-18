/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils

import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import androidx.browser.customtabs.CustomTabsIntent
import androidx.core.content.ContextCompat
import androidx.core.net.toUri
import androidx.fragment.app.FragmentActivity
import mozilla.components.browser.state.state.SessionState
import mozilla.components.feature.customtabs.createCustomTabConfigFromIntent
import mozilla.components.support.utils.ext.getPackageInfoCompat
import org.mozilla.focus.BuildConfig
import org.mozilla.focus.R
import org.mozilla.focus.activity.CustomTabActivity
import org.mozilla.focus.ext.components
import org.mozilla.focus.locale.Locales
import org.mozilla.focus.state.AppAction
import java.io.UnsupportedEncodingException
import java.net.URLEncoder
import java.util.Locale

object SupportUtils {
    const val HELP_URL = "https://support.mozilla.org/kb/what-firefox-focus-android"
    const val FOCUS_PLAY_STORE_URL = "https://play.google.com/store/apps/details?id=${BuildConfig.APPLICATION_ID}"
    const val RATE_APP_URL = "market://details?id=" + BuildConfig.APPLICATION_ID
    const val DEFAULT_BROWSER_URL = "https://support.mozilla.org/kb/set-firefox-focus-default-browser-android"
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
        internal val topicStr: String,
    ) {
        ADD_SEARCH_ENGINE("add-search-engine"),
        AUTOCOMPLETE("autofill-domain-android"),
        TRACKERS("trackers"),
        USAGE_DATA("usage-data"),
        SEARCH_SUGGESTIONS("search-suggestions-focus-android"),
        ALLOWLIST("focus-android-allowlist"),
        STUDIES("how-opt-out-studies-firefox-focus-android"),
        HTTPS_ONLY("https-only-prefs-focus"),
        COOKIE_BANNER("cookie-banner-reduction-firefox-focus-android"),
    }

    fun getGenericSumoURLForTopic(topic: SumoTopic): String {
        val escapedTopic = getEncodedTopicUTF8(topic.topicStr)
        val langTag = Locales.getLanguageTag(Locale.getDefault())
        return "https://support.mozilla.org/$langTag/kb/$escapedTopic"
    }

    /**
     * Returns the SUMO URL for a specific topic
     */
    fun getSumoURLForTopic(appVersion: String, topic: SumoTopic): String {
        val escapedTopic = getEncodedTopicUTF8(topic.topicStr)
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

    /**
     * Returns the version name of this package.
     */
    fun getAppVersion(context: Context): String {
        try {
            return context.packageManager.getPackageInfoCompat(context.packageName, 0).versionName
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
            private = true,
        )

        context.components.appStore.dispatch(
            AppAction.OpenTab(tabId),
        )
    }

    fun openUrlInCustomTab(activity: FragmentActivity, destinationUrl: String) {
        activity.intent.putExtra(
            CustomTabsIntent.EXTRA_TOOLBAR_COLOR,
            ContextCompat.getColor(activity, R.color.settings_background),
        )

        val tabId = activity.components.customTabsUseCases.add(
            url = destinationUrl,
            customTabConfig = createCustomTabConfigFromIntent(activity.intent, activity.resources),
            private = true,
            source = SessionState.Source.Internal.None,
        )
        val openCustomTabActivityIntent =
            Intent(activity, CustomTabActivity::class.java).apply {
                action = Intent.ACTION_VIEW
                data = getSumoURLForTopic(getAppVersion(activity), SumoTopic.ADD_SEARCH_ENGINE).toUri()
                putExtra(CustomTabActivity.CUSTOM_TAB_ID, tabId)
            }

        activity.startActivity(openCustomTabActivityIntent)
    }
}
