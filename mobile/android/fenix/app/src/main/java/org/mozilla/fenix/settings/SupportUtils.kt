/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import android.content.pm.PackageManager
import org.mozilla.fenix.BuildConfig
import java.io.UnsupportedEncodingException
import java.net.URLEncoder
import java.util.Locale

object SupportUtils {
    const val FEEDBACK_URL = "https://input.mozilla.org"
    const val RATE_APP_URL = "market://details?id=" + BuildConfig.APPLICATION_ID
    const val MOZILLA_MANIFESTO_URL = "https://www.mozilla.org/en-GB/about/manifesto/"

    enum class SumoTopic(
        internal val topicStr: String
    ) {
        HELP("firefox-android-help")
    }

    fun getSumoURLForTopic(context: Context, topic: SumoTopic): String {
        val escapedTopic = getEncodedTopicUTF8(topic.topicStr)
        val appVersion = getAppVersion(context)
        val osTarget = "Android"
        val langTag = Locale.getDefault().isO3Language
        return "https://support.mozilla.org/1/mobile/$appVersion/$osTarget/$langTag/$escapedTopic"
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
}
