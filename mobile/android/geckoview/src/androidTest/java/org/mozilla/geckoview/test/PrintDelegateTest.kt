/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.accessibilityservice.AccessibilityService
import android.app.UiAutomation.FLAG_DONT_SUPPRESS_ACCESSIBILITY_SERVICES
import android.content.Context
import android.graphics.Bitmap
import android.graphics.Color
import android.graphics.Color.rgb
import android.os.Handler
import android.os.Looper
import android.view.accessibility.AccessibilityEvent.TYPE_VIEW_SCROLLED
import androidx.test.ext.junit.rules.ActivityScenarioRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.LargeTest
import androidx.test.platform.app.InstrumentationRegistry
import org.hamcrest.CoreMatchers.containsString
import org.junit.After
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.rules.RuleChain
import org.junit.runner.RunWith
import org.mozilla.geckoview.Autofill
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoResult.fromException
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.GeckoPrintException
import org.mozilla.geckoview.GeckoSession.PrintDelegate
import org.mozilla.geckoview.GeckoView.ActivityContextDelegate
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule
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
    private val instrumentation = InstrumentationRegistry.getInstrumentation()
    private val uiAutomation = instrumentation.getUiAutomation(FLAG_DONT_SUPPRESS_ACCESSIBILITY_SERVICES)

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
            uiAutomation.setOnAccessibilityEventListener {}
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

    @NullDelegate(Autofill.Delegate::class)
    @Test
    fun windowDotPrintAvailableTest() {
        activityRule.scenario.onActivity {
            mainSession.loadTestPath(COLOR_ORANGE_BACKGROUND_HTML_PATH)
            mainSession.waitForPageStop()
            val response = mainSession.waitForJS("window.print();")
            assertTrue("Window.print(); is available.", response == null)
        }
    }

    // Returns the center pixel color of the the print preview's screenshot
    private fun printCenterPixelColor(): GeckoResult<Int> {
        val pixelResult = GeckoResult<Int>()
        // Listening for Android Print Activity
        uiAutomation.setOnAccessibilityEventListener { event ->
            if (event.packageName == "com.android.printspooler" &&
                event.eventType == TYPE_VIEW_SCROLLED
            ) {
                uiAutomation.setOnAccessibilityEventListener {}
                // Delaying the screenshot to give time for preview to load
                Handler(Looper.getMainLooper()).postDelayed({
                    val bitmap = uiAutomation.takeScreenshot()
                    val scaled = Bitmap.createScaledBitmap(bitmap, scaledWidth, scaledHeight, false)
                    pixelResult.complete(scaled.getPixel(scaledWidth / 2, scaledHeight / 2))
                }, 1500)
            }
        }
        return pixelResult
    }

    @NullDelegate(Autofill.Delegate::class)
    @Test
    fun printPreviewRendered() {
        activityRule.scenario.onActivity { activity ->
            // CSS rules render this blue on screen and orange on print
            mainSession.loadTestPath(PRINT_CONTENT_CHANGE)
            mainSession.waitForPageStop()
            // Setting to the default delegate (test rules changed it)
            mainSession.printDelegate = activity.view.printDelegate
            mainSession.printPageContent()
            val orange = rgb(255, 113, 57)
            val centerPixel = printCenterPixelColor()
            assertTrue(
                "Android print opened and rendered.",
                sessionRule.waitForResult(centerPixel) == orange,
            )
        }
    }

    @NullDelegate(Autofill.Delegate::class)
    @Test
    fun printSuccessWithStatus() {
        activityRule.scenario.onActivity { activity ->
            // CSS rules render this blue on screen and orange on print
            mainSession.loadTestPath(PRINT_CONTENT_CHANGE)
            mainSession.waitForPageStop()
            // Setting to the default delegate (test rules changed it)
            mainSession.printDelegate = activity.view.printDelegate
            val result = mainSession.didPrintPageContent()
            val orange = rgb(255, 113, 57)
            val centerPixel = printCenterPixelColor()
            assertTrue(
                "Android print opened and rendered.",
                sessionRule.waitForResult(centerPixel) == orange,
            )
            uiAutomation.performGlobalAction(AccessibilityService.GLOBAL_ACTION_BACK)
            assertTrue(
                "Printing should conclude when back is pressed.",
                sessionRule.waitForResult(result),
            )
        }
    }

    @Test
    fun printFailWithStatus() {
        activityRule.scenario.onActivity {
            // CSS rules render this blue on screen and orange on print
            mainSession.loadTestPath(PRINT_CONTENT_CHANGE)
            mainSession.waitForPageStop()
            mainSession.printDelegate = null
            val result = mainSession.didPrintPageContent().accept {
                assertTrue("Should not be able to print.", false)
            }.exceptionally(
                GeckoResult.OnExceptionListener<Throwable> { error: Throwable ->
                    assertTrue("Should receive a missing print delegate exception.", (error as GeckoPrintException).code == GeckoPrintException.ERROR_NO_PRINT_DELEGATE)
                    fromException(error)
                },
            )
            try {
                sessionRule.waitForResult(result)
            } catch (e: Exception) {
                assertTrue("Should have an exception", true)
            }
        }
    }

    @NullDelegate(Autofill.Delegate::class)
    @Test
    fun basicWindowDotPrintTest() {
        activityRule.scenario.onActivity { activity ->
            // CSS rules render this blue on screen and orange on print
            mainSession.loadTestPath(PRINT_CONTENT_CHANGE)
            mainSession.waitForPageStop()
            // Setting to the default delegate (test rules changed it)
            mainSession.printDelegate = activity.view.printDelegate
            mainSession.evaluateJS("window.print();")
            val centerPixel = printCenterPixelColor()
            val orange = rgb(255, 113, 57)
            assertTrue(
                "Android print opened and rendered.",
                sessionRule.waitForResult(centerPixel) == orange,
            )
        }
    }

    @NullDelegate(Autofill.Delegate::class)
    @Test
    fun statusWindowDotPrintTest() {
        activityRule.scenario.onActivity { activity ->
            // CSS rules render this blue on screen and orange on print
            mainSession.loadTestPath(PRINT_CONTENT_CHANGE)
            mainSession.waitForPageStop()
            // Setting to the default delegate (test rules changed it)
            mainSession.printDelegate = activity.view.printDelegate
            mainSession.evaluateJS("window.print()")
            val centerPixel = printCenterPixelColor()
            val orange = rgb(255, 113, 57)
            assertTrue(
                "Android print opened and rendered.",
                sessionRule.waitForResult(centerPixel) == orange,
            )
            var didCatch = false
            try {
                mainSession.evaluateJS("window.print();")
            } catch (e: GeckoSessionTestRule.RejectedPromiseException) {
                assertThat(
                    "Print status context reported.",
                    e.message,
                    containsString("Window.print: No browsing context"),
                )
                didCatch = true
            }
            assertTrue("Did show print status.", didCatch)
        }
    }

    @NullDelegate(Autofill.Delegate::class)
    @Test
    fun staticContextWindowDotPrintTest() {
        activityRule.scenario.onActivity { activity ->
            // CSS rules render this blue on screen and orange on print
            // Print button removes content after printing to test if it froze a static page for printing
            mainSession.loadTestPath(PRINT_CONTENT_CHANGE)
            mainSession.waitForPageStop()
            // Setting to the default delegate (test rules changed it)
            mainSession.printDelegate = activity.view.printDelegate
            mainSession.evaluateJS("document.getElementById('print-button').click();")
            val centerPixel = printCenterPixelColor()
            val orange = rgb(255, 113, 57)
            assertTrue(
                "Android print opened and rendered static page.",
                sessionRule.waitForResult(centerPixel) == orange,
            )
        }
    }

    @NullDelegate(Autofill.Delegate::class)
    @Test
    fun iframeWindowDotPrintTest() {
        activityRule.scenario.onActivity { activity ->
            // Main frame CSS rules render red on screen and green on print
            // iframe CSS rules render blue on screen and orange on print
            mainSession.loadTestPath(PRINT_IFRAME)
            mainSession.waitForPageStop()
            // Setting to the default delegate (test rules changed it)
            mainSession.printDelegate = activity.view.printDelegate
            // iframe window.print button
            mainSession.evaluateJS("document.getElementById('iframe').contentDocument.getElementById('print-button').click();")
            val centerPixelIframe = printCenterPixelColor()
            val orange = rgb(255, 113, 57)
            sessionRule.waitForResult(centerPixelIframe).let { it ->
                assertTrue("The iframe should not print green. (Printed containing page instead of iframe.)", it != Color.GREEN)
                assertTrue("Printed the iframe correctly.", it == orange)
            }
        }
    }

    @NullDelegate(Autofill.Delegate::class)
    @Test
    fun contentIframeWindowDotPrintTest() {
        activityRule.scenario.onActivity { activity ->
            // Main frame CSS rules render red on screen and green on print
            // iframe CSS rules render blue on screen and orange on print
            mainSession.loadTestPath(PRINT_IFRAME)
            mainSession.waitForPageStop()
            // Setting to the default delegate (test rules changed it)
            mainSession.printDelegate = activity.view.printDelegate
            // Main page window.print button
            mainSession.evaluateJS("document.getElementById('print-button-page').click();")
            val centerPixelContent = printCenterPixelColor()
            assertTrue("Printed the main content correctly.", sessionRule.waitForResult(centerPixelContent) == Color.GREEN)
        }
    }

    @NullDelegate(Autofill.Delegate::class)
    @Test
    fun contentPDFWindowDotPrintTest() {
        activityRule.scenario.onActivity { activity ->
            // CSS rules render this blue on screen and orange on print
            mainSession.loadTestPath(ORANGE_PDF_PATH)
            mainSession.waitForPageStop()
            // Setting to the default delegate (test rules changed it)
            mainSession.printDelegate = activity.view.printDelegate
            mainSession.printPageContent()
            val centerPixel = printCenterPixelColor()
            val orange = rgb(255, 113, 57)
            assertTrue(
                "Android print opened and rendered.",
                sessionRule.waitForResult(centerPixel) == orange,
            )
        }
    }

    @NullDelegate(Autofill.Delegate::class)
    @Test
    fun availableCanonicalBrowsingContext() {
        activityRule.scenario.onActivity { activity ->
            // CSS rules render this blue on screen and orange on print
            mainSession.loadTestPath(ORANGE_PDF_PATH)
            mainSession.waitForPageStop()
            // Setting to the default delegate (test rules changed it)
            mainSession.printDelegate = activity.view.printDelegate
            mainSession.setFocused(false)
            mainSession.printPageContent()
            val centerPixel = printCenterPixelColor()
            val orange = rgb(255, 113, 57)
            assertTrue(
                "Android print opened and rendered.",
                sessionRule.waitForResult(centerPixel) == orange,
            )
        }
    }
}
