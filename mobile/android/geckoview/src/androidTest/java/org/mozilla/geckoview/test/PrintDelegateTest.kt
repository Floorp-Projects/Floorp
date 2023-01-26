/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.content.Context
import androidx.test.ext.junit.rules.ActivityScenarioRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.LargeTest
import org.junit.After
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.rules.RuleChain
import org.junit.runner.RunWith
import org.mozilla.geckoview.Autofill
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.PrintDelegate
import org.mozilla.geckoview.GeckoView.ActivityContextDelegate
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.NullDelegate
import kotlin.math.roundToInt
@RunWith(AndroidJUnit4::class)
@LargeTest
class PrintDelegateTest : BaseSessionTest() {
    private val activityRule = ActivityScenarioRule(GeckoViewTestActivity::class.java)
    private var deviceHeight = 0
    private var deviceWidth = 0
    private var scaledHeight = 0
    private var scaledWidth = 12
//    See Bug 1812692:
//    private val instrumentation = InstrumentationRegistry.getInstrumentation()
//    private val uiAutomation = instrumentation.uiAutomation

    @get:Rule
    override val rules: RuleChain = RuleChain.outerRule(activityRule).around(sessionRule)

    @Before
    fun setup() {
        activityRule.scenario.onActivity {
            class PrintTestActivityDelegate : ActivityContextDelegate {
                override fun getActivityContext(): Context {
                    return it
                }
            }
            it.view.setSession(mainSession)
            // An activity delegate is required for printing
            it.view.activityContextDelegate = PrintTestActivityDelegate()
            deviceHeight = it.resources.displayMetrics.heightPixels
            deviceWidth = it.resources.displayMetrics.widthPixels
            scaledHeight = (scaledWidth * (deviceHeight / deviceWidth.toDouble())).roundToInt()
        }
    }

    @After
    fun cleanup() {
        activityRule.scenario.onActivity {
            it.view.releaseSession()
        }
    }

    @NullDelegate(Autofill.Delegate::class)
    @Test
    fun printDelegateTest() {
        activityRule.scenario.onActivity {
            var delegateCalled = 0
            sessionRule.delegateUntilTestEnd(object : PrintDelegate {
                @AssertCalled(count = 1)
                override fun onPrint(session: GeckoSession) {
                    delegateCalled++
                }
            })
            mainSession.loadTestPath(COLOR_ORANGE_BACKGROUND_HTML_PATH)
            mainSession.waitForPageStop()
            mainSession.printPageContent()
            assertTrue("Android print delegate called once.", delegateCalled == 1)
        }
    }
    /**
     *  ToDo: Bug 1812692 - Device screenshot method needs to be reconfigured before reopening test.
     *  The UIAutomation retrieval was causing intermittent failures in TextInputDelegateTest
     @NullDelegate(Autofill.Delegate::class)
     @Test
     fun printPreviewRendered() {
     activityRule.scenario.onActivity { activity ->
     // CSS rules render this blue on screen and orange on print
     mainSession.loadTestPath(COLOR_ORANGE_BACKGROUND_HTML_PATH)
     mainSession.waitForPageStop()
     // Setting to the default delegate (test rules changed it)
     mainSession.printDelegate = activity.view.printDelegate
     mainSession.printPageContent()
     // Wait for Print UI
     mainSession.waitForJS("new Promise(resolve => window.setTimeout(resolve, 1000))")
     val bitmap = uiAutomation.takeScreenshot()
     val scaled = Bitmap.createScaledBitmap(bitmap, scaledWidth, scaledHeight, false)
     val centerPixel = scaled.getPixel(scaledWidth / 2, scaledHeight / 2)
     // Only renders orange in print
     val orange = rgb(255, 113, 57)
     assertTrue("Android print opened and rendered.", centerPixel == orange)
     }
     }
     */
}
