/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.os.Parcel
import android.support.test.InstrumentationRegistry
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule

import org.hamcrest.Matcher
import org.hamcrest.Matchers
import org.junit.Rule
import org.junit.rules.ErrorCollector

import kotlin.reflect.KClass

/**
 * Common base class for tests using GeckoSessionTestRule,
 * providing the test rule and other utilities.
 */
open class BaseSessionTest(noErrorCollector: Boolean = false) {
    companion object {
        const val INVALID_URI = "http://www.test.invalid/"
        const val HELLO_HTML_PATH = "/assets/www/hello.html"
        const val HELLO2_HTML_PATH = "/assets/www/hello2.html"
        const val NEW_SESSION_HTML_PATH = "/assets/www/newSession.html";
        const val NEW_SESSION_CHILD_HTML_PATH = "/assets/www/newSession_child.html";
        const val CLICK_TO_RELOAD_HTML_PATH = "/assets/www/clickToReload.html";
    }

    @get:Rule val sessionRule = GeckoSessionTestRule()

    @get:Rule val errors = ErrorCollector()
    fun <T> assertThat(reason: String, v: T, m: Matcher<T>) = sessionRule.assertThat(reason, v, m)

    init {
        if (!noErrorCollector) {
            sessionRule.errorCollector = errors
        }
    }

    fun GeckoSession.getTestBytes(path: String) =
            InstrumentationRegistry.getTargetContext().resources.assets
                    .open(path.removePrefix("/assets/")).readBytes()

    fun GeckoSession.loadTestPath(path: String) =
            this.loadUri(GeckoSessionTestRule.APK_URI_PREFIX + path.removePrefix("/"))

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
}
