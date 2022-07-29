/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.os.Handler
import android.os.Looper
import android.provider.Settings
import android.text.format.DateFormat
import androidx.test.ext.junit.rules.ActivityScenarioRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import androidx.test.platform.app.InstrumentationRegistry
import org.hamcrest.Matchers.*
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.rules.RuleChain
import org.junit.runner.RunWith
import org.mozilla.gecko.GeckoAppShell
import org.mozilla.geckoview.AllowOrDeny
import org.mozilla.geckoview.Autofill
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule



@RunWith(AndroidJUnit4::class)
@MediumTest
class GeckoAppShellTest : BaseSessionTest() {
    private val activityRule = ActivityScenarioRule(GeckoViewTestActivity::class.java)
    private val context = InstrumentationRegistry.getInstrumentation().targetContext
    private var prior24HourSetting = true

    @get:Rule
    override val rules: RuleChain = RuleChain.outerRule(activityRule).around(sessionRule)

    @Before
    fun setup() {
        activityRule.scenario.onActivity {
            prior24HourSetting = DateFormat.is24HourFormat(context)
            it.view.setSession(sessionRule.session)
        }
    }

    @After
    fun cleanup() {
        activityRule.scenario.onActivity {
            // Return the test harness back to original setting
            setAndroid24HourTimeFormat(prior24HourSetting)
            it.view.releaseSession()
        }
    }

    // Sets the Android system is24HourFormat preference
    private fun setAndroid24HourTimeFormat(timeFormat: Boolean) {
        val setting = if (timeFormat) "24" else "12"
        Settings.System.putString(context.contentResolver, Settings.System.TIME_12_24, setting)
    }

    // Sends app to background, then to foreground, and finally loads a page
    private fun goHomeAndReturnWithPageLoad(){
        // Ensures a return to the foreground (onResume)
        Handler(Looper.getMainLooper()).postDelayed({
            sessionRule.requestActivityToForeground(context)
            // Will call onLoadRequest and allow test to finish
            mainSession.loadTestPath(HELLO_HTML_PATH)
            mainSession.waitForPageStop()
        }, 1500)

        // Will cause onPause event to occur
        sessionRule.simulatePressHome(context)
    }

    @GeckoSessionTestRule.NullDelegate(Autofill.Delegate::class)
    @Test
    fun testChange24HourClockSettings() {
        activityRule.scenario.onActivity {
            var onLoadRequestCount = 0;

            // First clock settings change, takes effect on next onResume
            // Time format that does not use AM/PM, e.g., 13:00
            setAndroid24HourTimeFormat(true)
            // Causes an onPause event, onResume event, and finally a page load request
            goHomeAndReturnWithPageLoad()

            // This is waiting and holding the test harness open while Android Lifecycle events complete
            mainSession.waitUntilCalled(object : GeckoSession.ContentDelegate, GeckoSession.NavigationDelegate {
                @GeckoSessionTestRule.AssertCalled(count = 2)
                override fun onLocationChange(
                        session: GeckoSession,
                        url: String?,
                        perms: MutableList<GeckoSession.PermissionDelegate.ContentPermission>) {
                    // Result of first clock settings change
                    if(onLoadRequestCount == 0) {
                        assertThat("Should use a 24 hour clock.",
                                GeckoAppShell.getIs24HourFormat(), equalTo(true))
                        onLoadRequestCount ++;

                        // Calling second clock settings change
                        // Time format that does use AM/PM, e.g., 1:00 PM
                        setAndroid24HourTimeFormat(false)
                        goHomeAndReturnWithPageLoad()

                    // Result of second clock settings change
                    } else {
                        assertThat("Should use a 12 hour clock.",
                                GeckoAppShell.getIs24HourFormat(), equalTo(false))
                    }
                }
            })
        }
    }
}
