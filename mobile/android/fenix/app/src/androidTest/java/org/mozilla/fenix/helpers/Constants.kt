/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

object Constants {

    // Device or AVD requires a Google Services Android OS installation
    object PackageName {
        const val GOOGLE_PLAY_SERVICES = "com.android.vending"
        const val GOOGLE_APPS_PHOTOS = "com.google.android.apps.photos"
        const val GOOGLE_QUICK_SEARCH = "com.google.android.googlequicksearchbox"
        const val YOUTUBE_APP = "com.google.android.youtube"
        const val GMAIL_APP = "com.google.android.gm"
        const val PHONE_APP = "com.android.dialer"
        const val ANDROID_SETTINGS = "com.android.settings"
    }

    const val SPEECH_RECOGNITION = "android.speech.action.RECOGNIZE_SPEECH"
    const val POCKET_RECOMMENDED_STORIES_UTM_PARAM = "utm_source=pocket-newtab-android"
    const val LONG_CLICK_DURATION: Long = 5000
    const val LISTS_MAXSWIPES: Int = 3
    const val RETRY_COUNT = 3

    val searchEngineCodes = mapOf(
        "Google" to "client=firefox-b-m",
        "Bing" to "firefox&pc=MOZB&form=MOZMBA",
        "DuckDuckGo" to "t=fpas",
    )
}
