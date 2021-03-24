/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import android.app.PendingIntent
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.net.Uri
import androidx.browser.customtabs.CustomTabsIntent
import androidx.preference.PreferenceManager
import androidx.test.espresso.Espresso
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.ActivityTestRule
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.R
import org.mozilla.focus.fragment.FirstrunFragment
import org.mozilla.focus.helpers.TestHelper.readTestAsset
import java.io.IOException

@RunWith(AndroidJUnit4ClassRunner::class)
@Ignore("Crashing the instrumentation tests, no session ID error")
class CustomTabTest {
    private var webServer: MockWebServer? = null
    private val MENU_ITEM_LABEL = "TestItem4223"
    private val ACTION_BUTTON_DESCRIPTION = "TestButton1337"
    private val TEST_PAGE_HEADER_ID = "header"
    private val TEST_PAGE_HEADER_TEXT = "focus test page"

    @get: Rule
    var activityTestRule = ActivityTestRule(
        IntentReceiverActivity::class.java, true, false
    )

    @Before
    fun setUp() {}

    @After
    fun tearDown() {
        stopWebServer()
    }

    @Test
    fun testCustomTabUI() {
        startWebServer()

        // Launch activity with custom tabs intent
        activityTestRule.launchActivity(createCustomTabIntent())

        // Wait for website to load

        // Verify action button is visible
        Espresso.onView(ViewMatchers.withContentDescription(ACTION_BUTTON_DESCRIPTION))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))

        // Open menu
        Espresso.onView(ViewMatchers.withId(R.id.menuView))
            .perform(ViewActions.click())

        // Verify share action is visible
        Espresso.onView(ViewMatchers.withId(R.id.share))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))

        // Verify custom menu item is visible
        Espresso.onView(ViewMatchers.withText(MENU_ITEM_LABEL))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))

        // Close the menu again
        Espresso.pressBack()

        // Verify close button is visible - Click it to close custom tab.
        Espresso.onView(ViewMatchers.withId(R.id.customtab_close))
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
            .perform(ViewActions.click())
    }

    @Suppress("Deprecation")
    private fun createCustomTabIntent(): Intent {
        val appContext = InstrumentationRegistry.getInstrumentation()
            .targetContext
            .applicationContext
        val pendingIntent = PendingIntent.getActivity(appContext, 0, Intent(), 0)
        val customTabsIntent = CustomTabsIntent.Builder()
            .addMenuItem(MENU_ITEM_LABEL, pendingIntent)
            .addDefaultShareMenuItem()
            .setActionButton(createTestBitmap(), ACTION_BUTTON_DESCRIPTION, pendingIntent, true)
            .setToolbarColor(Color.MAGENTA)
            .build()
        customTabsIntent.intent.data = Uri.parse(webServer!!.url("/").toString())
        return customTabsIntent.intent
    }

    private fun startWebServer() {
        val appContext = InstrumentationRegistry.getInstrumentation()
            .targetContext
            .applicationContext
        PreferenceManager.getDefaultSharedPreferences(appContext)
            .edit()
            .putBoolean(FirstrunFragment.FIRSTRUN_PREF, true)
            .apply()
        webServer = MockWebServer()
        try {
            webServer!!.enqueue(
                MockResponse()
                    .setBody(readTestAsset("plain_test.html"))
            )
            webServer!!.start()
        } catch (e: IOException) {
            throw AssertionError("Could not start web server", e)
        }
    }

    private fun stopWebServer() {
        try {
            webServer!!.close()
            webServer!!.shutdown()
        } catch (e: IOException) {
            throw AssertionError("Could not stop web server", e)
        }
    }

    private fun createTestBitmap(): Bitmap {
        val bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(bitmap)
        canvas.drawColor(Color.GREEN)
        return bitmap
    }
}
