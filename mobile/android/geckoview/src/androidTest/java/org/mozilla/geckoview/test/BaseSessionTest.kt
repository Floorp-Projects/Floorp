/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview.test

import android.os.Parcel
import androidx.test.platform.app.InstrumentationRegistry
import org.mozilla.geckoview.GeckoRuntimeSettings
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule

import org.hamcrest.Matcher
import org.hamcrest.Matchers
import org.json.JSONArray
import org.junit.Assume.assumeThat
import org.junit.Rule
import org.junit.rules.ErrorCollector

import kotlin.reflect.KClass

/**
 * Common base class for tests using GeckoSessionTestRule,
 * providing the test rule and other utilities.
 */
open class BaseSessionTest(noErrorCollector: Boolean = false) {
    companion object {
        const val CLICK_TO_RELOAD_HTML_PATH = "/assets/www/clickToReload.html"
        const val CONTENT_CRASH_URL = "about:crashcontent"
        const val DOWNLOAD_HTML_PATH = "/assets/www/download.html"
        const val FORMS_HTML_PATH = "/assets/www/forms.html"
        const val FORMS2_HTML_PATH = "/assets/www/forms2.html"
        const val FORMS3_HTML_PATH = "/assets/www/forms3.html"
        const val FORMS_AUTOCOMPLETE_HTML_PATH = "/assets/www/forms_autocomplete.html"
        const val HELLO_HTML_PATH = "/assets/www/hello.html"
        const val HELLO2_HTML_PATH = "/assets/www/hello2.html"
        const val HELLO_IFRAME_HTML_PATH = "/assets/www/iframe_hello.html"
        const val INPUTS_PATH = "/assets/www/inputs.html"
        const val INVALID_URI = "not a valid uri"
        const val LINKS_HTML_PATH = "/assets/www/links.html"
        const val LOREM_IPSUM_HTML_PATH = "/assets/www/loremIpsum.html"
        const val NEW_SESSION_CHILD_HTML_PATH = "/assets/www/newSession_child.html"
        const val NEW_SESSION_HTML_PATH = "/assets/www/newSession.html"
        const val POPUP_HTML_PATH = "/assets/www/popup.html"
        const val PROMPT_HTML_PATH = "/assets/www/prompts.html"
        const val SAVE_STATE_PATH = "/assets/www/saveState.html"
        const val TITLE_CHANGE_HTML_PATH = "/assets/www/titleChange.html"
        const val TRACKERS_PATH = "/assets/www/trackers.html"
        const val VIDEO_OGG_PATH = "/assets/www/ogg.html"
        const val VIDEO_MP4_PATH = "/assets/www/mp4.html"
        const val VIDEO_WEBM_PATH = "/assets/www/webm.html"
        const val VIDEO_BAD_PATH = "/assets/www/badVideoPath.html"
        const val UNKNOWN_HOST_URI = "http://www.test.invalid/"
        const val UNKNOWN_PROTOCOL_URI = "htt://invalid"
        const val FULLSCREEN_PATH = "/assets/www/fullscreen.html"
        const val VIEWPORT_PATH = "/assets/www/viewport.html"
        const val IFRAME_REDIRECT_LOCAL = "/assets/www/iframe_redirect_local.html"
        const val IFRAME_REDIRECT_AUTOMATION = "/assets/www/iframe_redirect_automation.html"
        const val AUTOPLAY_PATH = "/assets/www/autoplay.html"
        const val SCROLL_TEST_PATH = "/assets/www/scroll.html"
        const val COLORS_HTML_PATH = "/assets/www/colors.html"
        const val FIXED_BOTTOM = "/assets/www/fixedbottom.html"
        const val FIXED_VH = "/assets/www/fixedvh.html"
        const val FIXED_PERCENT = "/assets/www/fixedpercent.html"
        const val STORAGE_TITLE_HTML_PATH = "/assets/www/reflect_local_storage_into_title.html"
        const val HUNG_SCRIPT = "/assets/www/hungScript.html"
        const val PUSH_HTML_PATH = "/assets/www/push/push.html"
        const val OPEN_WINDOW_PATH = "/assets/www/worker/open_window.html"
        const val OPEN_WINDOW_TARGET_PATH = "/assets/www/worker/open_window_target.html"
        const val DATA_URI_PATH = "/assets/www/data_uri.html"

        const val TEST_ENDPOINT = GeckoSessionTestRule.TEST_ENDPOINT
    }

    @get:Rule val sessionRule = GeckoSessionTestRule()

    @get:Rule val errors = ErrorCollector()

    val mainSession get() = sessionRule.session

    fun <T> assertThat(reason: String, v: T, m: Matcher<in T>) = sessionRule.checkThat(reason, v, m)
    fun <T> assertInAutomationThat(reason: String, v: T, m: Matcher<in T>) =
            if (sessionRule.env.isAutomation) assertThat(reason, v, m)
            else assumeThat(reason, v, m)

    init {
        if (!noErrorCollector) {
            sessionRule.errorCollector = errors
        }
    }

    fun <T> forEachCall(vararg values: T): T = sessionRule.forEachCall(*values)

    fun getTestBytes(path: String) =
            InstrumentationRegistry.getInstrumentation().targetContext.resources.assets
                    .open(path.removePrefix("/assets/")).readBytes()

    fun createTestUrl(path: String) = GeckoSessionTestRule.TEST_ENDPOINT + path

    fun GeckoSession.loadTestPath(path: String) =
            this.loadUri(createTestUrl(path))

    inline fun GeckoSession.toParcel(lambda: (Parcel) -> Unit) {
        val parcel = Parcel.obtain()
        try {
            this.writeToParcel(parcel, 0)

            val pos = parcel.dataPosition()
            parcel.setDataPosition(0)

            lambda(parcel)

            assertThat("Read parcel matches written parcel",
                       parcel.dataPosition(), Matchers.equalTo(pos))
        } finally {
            parcel.recycle()
        }
    }

    inline fun GeckoRuntimeSettings.toParcel(lambda: (Parcel) -> Unit) {
        val parcel = Parcel.obtain()
        try {
            this.writeToParcel(parcel, 0)

            val pos = parcel.dataPosition()
            parcel.setDataPosition(0)

            lambda(parcel)

            assertThat("Read parcel matches written parcel",
                       parcel.dataPosition(), Matchers.equalTo(pos))
        } finally {
            parcel.recycle()
        }
    }

    fun GeckoSession.open() =
            sessionRule.openSession(this)

    fun GeckoSession.waitForPageStop() =
            sessionRule.waitForPageStop(this)

    fun GeckoSession.waitForPageStops(count: Int) =
            sessionRule.waitForPageStops(this, count)

    fun GeckoSession.waitUntilCalled(ifce: KClass<*>, vararg methods: String) =
            sessionRule.waitUntilCalled(this, ifce, *methods)

    fun GeckoSession.waitUntilCalled(callback: Any) =
            sessionRule.waitUntilCalled(this, callback)

    fun GeckoSession.forCallbacksDuringWait(callback: Any) =
            sessionRule.forCallbacksDuringWait(this, callback)

    fun GeckoSession.delegateUntilTestEnd(callback: Any) =
            sessionRule.delegateUntilTestEnd(this, callback)

    fun GeckoSession.delegateDuringNextWait(callback: Any) =
            sessionRule.delegateDuringNextWait(this, callback)

    fun GeckoSession.synthesizeTap(x: Int, y: Int) =
            sessionRule.synthesizeTap(this, x, y)

    fun GeckoSession.evaluateJS(js: String): Any? =
            sessionRule.evaluateJS(this, js)

    fun GeckoSession.evaluatePromiseJS(js: String): GeckoSessionTestRule.ExtensionPromise =
            sessionRule.evaluatePromiseJS(this, js)

    fun GeckoSession.waitForJS(js: String): Any? =
            sessionRule.waitForJS(this, js)

    fun GeckoSession.waitForRoundTrip() = sessionRule.waitForRoundTrip(this)

    @Suppress("UNCHECKED_CAST")
    fun Any?.asJsonArray(): JSONArray = this as JSONArray

    @Suppress("UNCHECKED_CAST")
    fun<T> Any?.asJSList(): List<T> {
        val array = this.asJsonArray()
        val result = ArrayList<T>()

        for (i in 0 until array.length()) {
            result.add(array[i] as T)
        }

        return result
    }
}
