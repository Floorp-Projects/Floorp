/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import android.content.Intent
import androidx.browser.customtabs.CustomTabColorSchemeParams
import androidx.browser.customtabs.CustomTabsIntent
import androidx.core.net.toUri
import mozilla.components.support.ktx.android.content.appVersionName
import mozilla.components.support.ktx.android.content.getColorFromAttr
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.IntentReceiverActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.settings.account.AuthIntentReceiverActivity
import java.io.UnsupportedEncodingException
import java.net.URLEncoder
import java.util.Locale

object SupportUtils {
    const val RATE_APP_URL = "market://details?id=" + BuildConfig.APPLICATION_ID
    const val POCKET_TRENDING_URL = "https://getpocket.com/fenix-top-articles"
    const val WIKIPEDIA_URL = "https://www.wikipedia.org/"
    const val FENIX_PLAY_STORE_URL = "https://play.google.com/store/apps/details?id=${BuildConfig.APPLICATION_ID}"
    const val GOOGLE_URL = "https://www.google.com/"
    const val BAIDU_URL = "https://m.baidu.com/?from=1000969a"
    const val JD_URL = "https://union-click.jd.com/jdc" +
        "?e=&p=AyIGZRprFDJWWA1FBCVbV0IUWVALHFRBEwQAQB1AWQkFVUVXfFkAF14lRFRbJXstVWR3WQ1rJ08AZnhS" +
        "HDJBYh4LZR9eEAMUBlccWCUBEQZRGFoXCxc3ZRteJUl8BmUZWhQ" +
        "AEwdRGF0cMhIAVB5ZFAETBVAaXRwyFQdcKydLSUpaCEtYFAIXN2UrWCUyIgdVK1slXVZaCCtZFAMWDg%3D%3D"
    const val PDD_URL = "https://mobile.yangkeduo.com/duo_cms_mall.html?pid=13289095_194240604&" +
        "cpsSign=CM_210309_13289095_194240604_8bcfd56d5db3c43d983014d2658ec26e&duoduo_type=2"
    const val TC_URL = "https://jumpluna.58.com/i/29HU"
    const val MEITUAN_URL = "https://tb.j5k6.com/6ZSOp"
    const val GOOGLE_US_URL = "https://www.google.com/webhp?client=firefox-b-1-m&channel=ts"
    const val GOOGLE_XX_URL = "https://www.google.com/webhp?client=firefox-b-m&channel=ts"

    enum class SumoTopic(internal val topicStr: String) {
        HELP("faq-android"),
        PRIVATE_BROWSING_MYTHS("common-myths-about-private-browsing"),
        YOUR_RIGHTS("your-rights"),
        TRACKING_PROTECTION("tracking-protection-firefox-android"),
        TOTAL_COOKIE_PROTECTION("enhanced-tracking-protection-android"),
        WHATS_NEW("whats-new-firefox-preview"),
        OPT_OUT_STUDIES("how-opt-out-studies-firefox-android"),
        SEND_TABS("send-tab-preview"),
        SET_AS_DEFAULT_BROWSER("set-firefox-preview-default"),
        SEARCH_SUGGESTION("how-search-firefox-preview"),
        CUSTOM_SEARCH_ENGINES("custom-search-engines"),
        SYNC_SETUP("how-set-firefox-sync-firefox-android"),
        QR_CAMERA_ACCESS("qr-camera-access"),
        SMARTBLOCK("smartblock-enhanced-tracking-protection"),
        SPONSOR_PRIVACY("sponsor-privacy"),
        HTTPS_ONLY_MODE("https-only-mode-firefox-android"),
        COOKIE_BANNER("cookie-banner-reduction-firefox-android"),
    }

    enum class MozillaPage(internal val path: String) {
        PRIVATE_NOTICE("privacy/firefox/"),
        MANIFESTO("about/manifesto/"),
    }

    /**
     * Gets a support page URL for the corresponding topic.
     */
    fun getSumoURLForTopic(
        context: Context,
        topic: SumoTopic,
        locale: Locale = Locale.getDefault(),
    ): String {
        val escapedTopic = getEncodedTopicUTF8(topic.topicStr)
        // Remove the whitespace so a search is not triggered:
        val appVersion = context.appVersionName.replace(" ", "")
        val osTarget = "Android"
        val langTag = getLanguageTag(locale)
        return "https://support.mozilla.org/1/mobile/$appVersion/$osTarget/$langTag/$escapedTopic"
    }

    /**
     * Gets a support page URL for the corresponding topic.
     * Used when the app version and os are not part of the URL.
     */
    fun getGenericSumoURLForTopic(topic: SumoTopic, locale: Locale = Locale.getDefault()): String {
        val escapedTopic = getEncodedTopicUTF8(topic.topicStr)
        val langTag = getLanguageTag(locale)
        return "https://support.mozilla.org/$langTag/kb/$escapedTopic"
    }

    fun getFirefoxAccountSumoUrl(): String {
        return "https://support.mozilla.org/kb/access-mozilla-services-firefox-account"
    }

    fun getMozillaPageUrl(page: MozillaPage, locale: Locale = Locale.getDefault()): String {
        val path = page.path
        val langTag = getLanguageTag(locale)
        return "https://www.mozilla.org/$langTag/$path"
    }

    fun getWhatsNewUrl(context: Context) = getSumoURLForTopic(context, SumoTopic.WHATS_NEW)

    fun createCustomTabIntent(context: Context, url: String): Intent = CustomTabsIntent.Builder()
        .setInstantAppsEnabled(false)
        .setDefaultColorSchemeParams(
            CustomTabColorSchemeParams.Builder().setToolbarColor(context.getColorFromAttr(R.attr.layer1)).build(),
        )
        .build()
        .intent
        .setData(url.toUri())
        .setClassName(context, IntentReceiverActivity::class.java.name)
        .setPackage(context.packageName)

    fun createAuthCustomTabIntent(context: Context, url: String): Intent =
        createCustomTabIntent(context, url).setClassName(context, AuthIntentReceiverActivity::class.java.name)

    private fun getEncodedTopicUTF8(topic: String): String {
        try {
            return URLEncoder.encode(topic, "UTF-8")
        } catch (e: UnsupportedEncodingException) {
            throw IllegalStateException("utf-8 should always be available", e)
        }
    }

    private fun getLanguageTag(locale: Locale): String {
        val language = locale.language
        val country = locale.country // Can be an empty string.
        return if (country.isEmpty()) language else "$language-$country"
    }
}
