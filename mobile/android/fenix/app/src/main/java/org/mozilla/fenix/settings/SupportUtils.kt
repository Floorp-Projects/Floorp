/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.IntentReceiverActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getColorFromAttr
import java.io.UnsupportedEncodingException
import java.net.URLEncoder
import java.util.Locale

object SupportUtils {
    const val RATE_APP_URL = "market://details?id=" + BuildConfig.APPLICATION_ID
    const val MOZILLA_MANIFESTO_URL = "https://www.mozilla.org/en-GB/about/manifesto/"
    const val FENIX_PLAY_STORE_URL = "https://play.google.com/store/apps/details?id=${BuildConfig.APPLICATION_ID}"
    val PRIVACY_NOTICE_URL: String
        get() = "https://www.mozilla.org/${getLanguageTag(Locale.getDefault())}/privacy/firefox/"

    enum class SumoTopic(
        internal val topicStr: String
    ) {
        HELP("faq-android"),
        PRIVATE_BROWSING_MYTHS("common-myths-about-private-browsing"),
        YOUR_RIGHTS("your-rights"),
        TRACKING_PROTECTION("tracking-protection-firefox-preview")
    }

    fun getSumoURLForTopic(context: Context, topic: SumoTopic): String {
        val escapedTopic = getEncodedTopicUTF8(topic.topicStr)
        val appVersion = getAppVersion(context)
        val osTarget = "Android"
        val langTag = getLanguageTag(Locale.getDefault())
        return "https://support.mozilla.org/1/mobile/$appVersion/$osTarget/$langTag/$escapedTopic"
    }

    // Used when the app version and os are not part of the URL
    fun getGenericSumoURLForTopic(topic: SumoTopic): String {
        val escapedTopic = getEncodedTopicUTF8(topic.topicStr)
        val langTag = getLanguageTag(Locale.getDefault())
        return "https://support.mozilla.org/$langTag/kb/$escapedTopic"
    }

    fun createCustomTabIntent(context: Context, url: String) = Intent(Intent.ACTION_VIEW).apply {
        putExtra(context.getString(R.string.intent_extra_toolbar_color), R.attr.foundation.getColorFromAttr(context))
        putExtra(context.getString(R.string.intent_extra_session), true)
        setClassName(context.applicationContext, IntentReceiverActivity::class.java.name)
        data = Uri.parse(url)
        setPackage(context.packageName)
    }

    fun createAuthCustomTabIntent(context: Context, url: String) = Intent(Intent.ACTION_VIEW).apply {
        putExtra(context.getString(R.string.intent_extra_toolbar_color), R.attr.foundation.getColorFromAttr(context))
        putExtra(context.getString(R.string.intent_extra_session), true)
        putExtra(context.getString(R.string.intent_extra_auth), true)
        setClassName(context.applicationContext, IntentReceiverActivity::class.java.name)
        data = Uri.parse(url)
        setPackage(context.packageName)
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
            throw IllegalStateException("Unable find package details for Fenix", e)
        }
    }

    private fun getLanguageTag(locale: Locale): String {
        val language = locale.language
        val country = locale.country // Can be an empty string.
        return if (country == "") {
            language
        } else "$language-$country"
    }
}
