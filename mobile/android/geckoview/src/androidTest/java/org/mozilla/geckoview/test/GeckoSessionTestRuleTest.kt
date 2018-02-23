/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSessionSettings
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.Setting
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.TimeoutMillis
import org.mozilla.geckoview.test.util.Callbacks

import android.support.test.filters.LargeTest
import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import org.hamcrest.CoreMatchers.both
import org.hamcrest.Matchers.greaterThan
import org.hamcrest.Matchers.lessThan
import org.junit.Assert.*

import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Test for the GeckoSessionTestRule class, to ensure it properly sets up a session for
 * each test, and to ensure it can properly wait for and assert listener/delegate
 * callbacks.
 */
@RunWith(AndroidJUnit4::class)
@MediumTest
class GeckoSessionTestRuleTest {
    companion object {
        const val HELLO_URI = GeckoSessionTestRule.APK_URI_PREFIX + "/assets/www/hello.html"
    }

    @get:Rule val sessionRule = GeckoSessionTestRule()

    @Test fun getSession() {
        assertNotNull("Can get session", sessionRule.session)
        assertTrue("Session is open", sessionRule.session.isOpen)
    }

    @Setting.List(Setting(key = Setting.Key.USE_PRIVATE_MODE, value = "true"),
                  Setting(key = Setting.Key.DISPLAY_MODE, value = "DISPLAY_MODE_MINIMAL_UI"))
    @Test fun settingsApplied() {
        assertEquals("USE_PRIVATE_MODE should be set", true,
                     sessionRule.session.settings.getBoolean(GeckoSessionSettings.USE_PRIVATE_MODE))
        assertEquals("DISPLAY_MODE should be set",
                     GeckoSessionSettings.DISPLAY_MODE_MINIMAL_UI,
                     sessionRule.session.settings.getInt(GeckoSessionSettings.DISPLAY_MODE))
    }

    @Test(expected = AssertionError::class)
    @TimeoutMillis(1000)
    @LargeTest
    fun noPendingCallbacks() {
        // Make sure we don't have unexpected pending callbacks at the start of a test.
        sessionRule.waitUntilCalled(object : Callbacks.All {})
    }

    @Test fun waitForPageStop() {
        sessionRule.session.loadUri(HELLO_URI)
        sessionRule.waitForPageStop()

        var counter = 0

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressListener {
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        assertEquals("Callback count should be correct", 1, counter)
    }

    @Test(expected = AssertionError::class)
    fun waitForPageStop_throwOnChangedCallback() {
        sessionRule.session.progressListener = Callbacks.Default
        sessionRule.session.loadUri(HELLO_URI)
        sessionRule.waitForPageStop()
    }

    @Test fun waitForPageStops() {
        sessionRule.session.loadUri(HELLO_URI)
        sessionRule.session.reload()
        sessionRule.waitForPageStops(2)

        var counter = 0

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressListener {
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        assertEquals("Callback count should be correct", 2, counter)
    }

    @Test fun waitUntilCalled_anyInterfaceMethod() {
        sessionRule.session.loadUri(HELLO_URI)
        sessionRule.waitUntilCalled(GeckoSession.ProgressListener::class.java)

        var counter = 0

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressListener {
            override fun onPageStart(session: GeckoSession, url: String) {
                counter++
            }

            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }

            override fun onSecurityChange(session: GeckoSession,
                                          securityInfo: GeckoSession.ProgressListener.SecurityInformation) {
                counter++
            }
        })

        assertEquals("Callback count should be correct", 1, counter)
    }

    @Test fun waitUntilCalled_specificInterfaceMethod() {
        sessionRule.session.loadUri(HELLO_URI)
        sessionRule.waitUntilCalled(GeckoSession.ProgressListener::class.java,
                                     "onPageStart", "onPageStop")

        var counter = 0

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressListener {
            override fun onPageStart(session: GeckoSession, url: String) {
                counter++
            }

            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        assertEquals("Callback count should be correct", 2, counter)
    }

    @Test(expected = AssertionError::class)
    fun waitUntilCalled_throwOnNotGeckoSessionInterface() {
        sessionRule.session.loadUri(HELLO_URI)
        sessionRule.waitUntilCalled(CharSequence::class.java)
    }

    @Test fun waitUntilCalled_anyObjectMethod() {
        sessionRule.session.loadUri(HELLO_URI)

        var counter = 0

        sessionRule.waitUntilCalled(object : Callbacks.ProgressListener {
            override fun onPageStart(session: GeckoSession, url: String) {
                counter++
            }

            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }

            override fun onSecurityChange(session: GeckoSession,
                                          securityInfo: GeckoSession.ProgressListener.SecurityInformation) {
                counter++
            }
        })

        assertEquals("Callback count should be correct", 1, counter)
    }

    @Test fun waitUntilCalled_specificObjectMethod() {
        sessionRule.session.loadUri(HELLO_URI)

        var counter = 0

        sessionRule.waitUntilCalled(object : Callbacks.ProgressListener {
            @AssertCalled
            override fun onPageStart(session: GeckoSession, url: String) {
                counter++
            }

            @AssertCalled
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        assertEquals("Callback count should be correct", 2, counter)
    }

    @Test fun waitUntilCalled_multipleCount() {
        sessionRule.session.loadUri(HELLO_URI)
        sessionRule.session.reload()

        var counter = 0

        sessionRule.waitUntilCalled(object : Callbacks.ProgressListener {
            @AssertCalled(count = 2)
            override fun onPageStart(session: GeckoSession, url: String) {
                counter++
            }

            @AssertCalled(count = 2)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        assertEquals("Callback count should be correct", 4, counter)
    }

    @Test fun waitUntilCalled_currentCall() {
        sessionRule.session.loadUri(HELLO_URI)
        sessionRule.session.reload()

        var counter = 0

        sessionRule.waitUntilCalled(object : Callbacks.ProgressListener {
            @AssertCalled(count = 2, order = intArrayOf(1, 2))
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                val info = sessionRule.currentCall
                assertNotNull("Method info should be valid", info)
                assertThat("Counter should be correct", info.counter,
                        both(greaterThan(0)).and(lessThan(3)))
                assertEquals("Order should equal counter", info.counter, info.order)
                counter++
            }
        })

        assertEquals("Callback count should be correct", 2, counter)
    }

    @Test fun forCallbacksDuringWait_anyMethod() {
        sessionRule.session.loadUri(HELLO_URI)
        sessionRule.waitForPageStop()

        var counter = 0

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressListener {
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        assertEquals("Callback count should be correct", 1, counter)
    }

    @Test(expected = AssertionError::class)
    fun forCallbacksDuringWait_throwOnAnyMethodNotCalled() {
        sessionRule.session.loadUri(HELLO_URI)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(GeckoSession.ScrollListener { _, _, _ -> })
    }

    @Test fun forCallbacksDuringWait_specificMethod() {
        sessionRule.session.loadUri(HELLO_URI)
        sessionRule.waitForPageStop()

        var counter = 0

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressListener {
            @AssertCalled
            override fun onPageStart(session: GeckoSession, url: String) {
                counter++
            }

            @AssertCalled
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        assertEquals("Callback count should be correct", 2, counter)
    }

    @Test fun forCallbacksDuringWait_specificMethodMultipleTimes() {
        sessionRule.session.loadUri(HELLO_URI)
        sessionRule.session.reload()
        sessionRule.waitForPageStops(2)

        var counter = 0

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressListener {
            @AssertCalled
            override fun onPageStart(session: GeckoSession, url: String) {
                counter++
            }

            @AssertCalled
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        assertEquals("Callback count should be correct", 4, counter)
    }

    @Test(expected = AssertionError::class)
    fun forCallbacksDuringWait_throwOnSpecificMethodNotCalled() {
        sessionRule.session.loadUri(HELLO_URI)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(
                GeckoSession.ScrollListener @AssertCalled { _, _, _ -> })
    }

    @Test fun forCallbacksDuringWait_specificCount() {
        sessionRule.session.loadUri(HELLO_URI)
        sessionRule.session.reload()
        sessionRule.waitForPageStops(2)

        var counter = 0

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressListener {
            @AssertCalled(count = 2)
            override fun onPageStart(session: GeckoSession, url: String) {
                counter++
            }

            @AssertCalled(count = 2)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        assertEquals("Callback count should be correct", 4, counter)
    }

    @Test(expected = AssertionError::class)
    fun forCallbacksDuringWait_throwOnWrongCount() {
        sessionRule.session.loadUri(HELLO_URI)
        sessionRule.session.reload()
        sessionRule.waitForPageStops(2)

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressListener {
            @AssertCalled(count = 1)
            override fun onPageStart(session: GeckoSession, url: String) {
            }

            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })
    }

    @Test fun forCallbacksDuringWait_specificOrder() {
        sessionRule.session.loadUri(HELLO_URI)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressListener {
            @AssertCalled(order = intArrayOf(1))
            override fun onPageStart(session: GeckoSession, url: String) {
            }

            @AssertCalled(order = intArrayOf(2))
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })
    }

    @Test(expected = AssertionError::class)
    fun forCallbacksDuringWait_throwOnWrongOrder() {
        sessionRule.session.loadUri(HELLO_URI)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressListener {
            @AssertCalled(order = intArrayOf(2))
            override fun onPageStart(session: GeckoSession, url: String) {
            }

            @AssertCalled(order = intArrayOf(1))
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })
    }

    @Test fun forCallbacksDuringWait_multipleOrder() {
        sessionRule.session.loadUri(HELLO_URI)
        sessionRule.session.reload()
        sessionRule.waitForPageStops(2)

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressListener {
            @AssertCalled(order = intArrayOf(1, 3, 1))
            override fun onPageStart(session: GeckoSession, url: String) {
            }

            @AssertCalled(order = intArrayOf(2, 4, 1))
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })
    }

    @Test(expected = AssertionError::class)
    fun forCallbacksDuringWait_throwOnWrongMultipleOrder() {
        sessionRule.session.loadUri(HELLO_URI)
        sessionRule.session.reload()
        sessionRule.waitForPageStops(2)

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressListener {
            @AssertCalled(order = intArrayOf(1, 2, 1))
            override fun onPageStart(session: GeckoSession, url: String) {
            }

            @AssertCalled(order = intArrayOf(3, 4, 1))
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })
    }

    @Test fun forCallbacksDuringWait_notCalled() {
        sessionRule.session.loadUri(HELLO_URI)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(
                GeckoSession.ScrollListener @AssertCalled(false) { _, _, _ -> })
    }

    @Test(expected = AssertionError::class)
    fun forCallbacksDuringWait_throwOnCallingNoCall() {
        sessionRule.session.loadUri(HELLO_URI)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressListener {
            @AssertCalled(false)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
            }
        })
    }

    @Test fun forCallbacksDuringWait_limitedToLastWait() {
        sessionRule.session.loadUri(HELLO_URI)
        sessionRule.session.reload()
        sessionRule.session.reload()
        sessionRule.session.reload()

        // Wait for Gecko to finish all loads.
        Thread.sleep(100)

        sessionRule.waitForPageStop() // Wait for loadUri.
        sessionRule.waitForPageStop() // Wait for first reload.

        var counter = 0

        // assert should only apply to callbacks within range (loadUri, first reload].
        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressListener {
            @AssertCalled(count = 1)
            override fun onPageStart(session: GeckoSession, url: String) {
                counter++
            }

            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                counter++
            }
        })

        assertEquals("Callback count should be correct", 2, counter)
    }

    @Test fun forCallbacksDuringWait_currentCall() {
        sessionRule.session.loadUri(HELLO_URI)
        sessionRule.waitForPageStop()

        sessionRule.forCallbacksDuringWait(object : Callbacks.ProgressListener {
            @AssertCalled(count = 1)
            override fun onPageStop(session: GeckoSession, success: Boolean) {
                val info = sessionRule.currentCall
                assertNotNull("Method info should be valid", info)
                assertEquals("Counter should be correct", 1, info.counter)
                assertEquals("Order should equal counter", 0, info.order)
            }
        })
    }

    @Test(expected = AssertionError::class)
    fun getCurrentCall_throwOnNoCurrentCall() {
        sessionRule.currentCall
    }
}