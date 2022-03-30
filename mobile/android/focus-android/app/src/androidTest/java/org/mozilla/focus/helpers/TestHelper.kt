/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.helpers

import android.app.PendingIntent
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.net.Uri
import android.text.format.DateUtils
import android.util.Log
import android.view.KeyEvent
import androidx.browser.customtabs.CustomTabsIntent
import androidx.test.espresso.Espresso
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.intent.Intents.intended
import androidx.test.espresso.intent.matcher.IntentMatchers.toPackage
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.espresso.web.sugar.Web
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiSelector
import junit.framework.AssertionFailedError
import okhttp3.mockwebserver.MockResponse
import okio.Buffer
import org.hamcrest.Matchers
import org.hamcrest.Matchers.allOf
import org.junit.Assert
import org.junit.Assert.assertTrue
import org.mozilla.focus.R
import org.mozilla.focus.activity.IntentReceiverActivity
import org.mozilla.focus.utils.AppConstants.isKlarBuild
import org.mozilla.focus.utils.IntentUtils
import java.io.BufferedReader
import java.io.File
import java.io.FileInputStream
import java.io.IOException
import java.io.InputStream
import java.io.InputStreamReader
import java.nio.charset.StandardCharsets

@Suppress("TooManyFunctions")
object TestHelper {
    @JvmField
    var mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
    const val waitingTime = DateUtils.SECOND_IN_MILLIS * 15
    const val pageLoadingTime = DateUtils.SECOND_IN_MILLIS * 25
    const val waitingTimeShort = DateUtils.SECOND_IN_MILLIS * 5

    @JvmStatic
    val getTargetContext: Context = InstrumentationRegistry.getInstrumentation().targetContext

    @JvmStatic
    val packageName: String = getTargetContext.packageName

    @JvmStatic
    val appName: String = getTargetContext.getString(R.string.app_name)

    fun getStringResource(id: Int) = getTargetContext.resources.getString(id, appName)

    fun verifySnackBarText(text: String) {
        val snackbarText = mDevice.findObject(UiSelector().textContains(text))
        assertTrue(snackbarText.waitForExists(waitingTime))
    }

    fun clickSnackBarActionButton(action: String) {
        val snackbarActionButton =
            onView(
                allOf(
                    withId(R.id.snackbar_action),
                    withText(action)
                )
            )
        snackbarActionButton.perform(click())
    }

    fun waitUntilSnackBarGone() {
        mDevice.findObject(UiSelector().resourceId("$appName:id/snackbar_layout"))
            .waitUntilGone(waitingTime)
    }

    fun isPackageInstalled(packageName: String): Boolean {
        return try {
            val packageManager = InstrumentationRegistry.getInstrumentation().context.packageManager
            packageManager.getApplicationInfo(packageName, 0).enabled
        } catch (exception: PackageManager.NameNotFoundException) {
            Log.d("TestLog", exception.message.toString())
            false
        }
    }

    fun restartApp(activity: MainActivityFirstrunTestRule) {
        with(activity) {
            finishActivity()
            mDevice.waitForIdle()
            launchActivity(null)
        }
    }

    // exit to the main view
    fun exitToTop() {
        val homeScreen =
            mDevice.findObject(UiSelector().resourceId("$packageName:id/landingLayout"))
        var homeScreenVisible = false
        while (!homeScreenVisible) {
            mDevice.pressBack()
            homeScreenVisible = homeScreen.waitForExists(2000)
        }
    }

    // exit to the browser view
    fun exitToBrowser() {
        val browserScreen =
            mDevice.findObject(UiSelector().resourceId("$packageName:id/main_content"))
        var browserScreenVisible = false
        while (!browserScreenVisible) {
            mDevice.pressBack()
            browserScreenVisible = browserScreen.waitForExists(2000)
        }
    }

    fun setNetworkEnabled(enabled: Boolean) {
        when (enabled) {
            true -> {
                mDevice.executeShellCommand("svc data enable")
                mDevice.executeShellCommand("svc wifi enable")
            }

            false -> {
                mDevice.executeShellCommand("svc data disable")
                mDevice.executeShellCommand("svc wifi disable")
            }
        }
        mDevice.waitForIdle(waitingTime)
    }

    // verifies localized strings in different UIs
    fun verifyTranslatedTextExists(text: String) =
        assertTrue(mDevice.findObject(UiSelector().text(text)).waitForExists(waitingTime))

    // wait for web area to be visible
    @JvmStatic
    fun waitForWebContent() {
        Assert.assertTrue(geckoView.waitForExists(waitingTime))
    }

    /********* Main View Locators  */
    @JvmField
    var menuButton = Espresso.onView(
        Matchers.allOf(
            ViewMatchers.withId(R.id.menuView),
            ViewMatchers.isDisplayed()
        )
    )

    @JvmField
    var permAllowBtn = mDevice.findObject(
        UiSelector()
            .textContains("Allow")
            .clickable(true)
    )

    @JvmField
    var inlineAutocompleteEditText = mDevice.findObject(
        UiSelector()
            .resourceId(packageName + ":id/urlView")
            .focused(true)
            .enabled(true)
    )

    @JvmField
    var hint = mDevice.findObject(
        UiSelector()
            .resourceId(packageName + ":id/searchView")
            .clickable(true)
    )

    @JvmField
    var webView = mDevice.findObject(
        UiSelector()
            .className("android.webkit.WebView")
            .enabled(true)
    )
    var geckoView = mDevice.findObject(
        UiSelector()
            .resourceId(packageName + ":id/engineView")
            .enabled(true)
    )

    @JvmField
    var progressBar = mDevice.findObject(
        UiSelector()
            .resourceId(packageName + ":id/progress")
            .enabled(true)
    )

    @JvmField
    var floatingEraseButton = Espresso.onView(
        // Replace -1 with the real id of erase button
        Matchers.allOf(ViewMatchers.withId(-1), ViewMatchers.isDisplayed())
    )

    @JvmField
    var erasedMsg = mDevice.findObject(
        UiSelector()
            .text("Your browsing history has been erased.")
            .resourceId(packageName + ":id/snackbar_text")
            .enabled(true)
    )

    @JvmField
    var AddtoHSmenuItem = mDevice.findObject(
        UiSelector()
            .resourceId(packageName + ":id/add_to_homescreen")
            .enabled(true)
    )

    @JvmField
    var AddtoHSCancelBtn = mDevice.findObject(
        UiSelector()
            .resourceId(packageName + ":id/addtohomescreen_dialog_cancel")
            .enabled(true)
    )

    @JvmField
    var savedNotification = mDevice.findObject(
        UiSelector()
            .text("Download complete.")
            .resourceId("android:id/text")
            .enabled(true)
    )

    @JvmField
    var securityInfoIcon = mDevice.findObject(
        UiSelector()
            .resourceId(packageName + ":id/security_info")
            .enabled(true)
    )

    @JvmField
    var identityState = mDevice.findObject(
        UiSelector()
            .resourceId(packageName + ":id/site_identity_state")
            .enabled(true)
    )

    @JvmField
    var downloadTitle = mDevice.findObject(
        UiSelector()
            .resourceId(packageName + ":id/title_template")
            .enabled(true)
    )

    @JvmField
    var downloadBtn = mDevice.findObject(
        UiSelector()
            .resourceId(packageName + ":id/download_dialog_download")
            .enabled(true)
    )

    /********** Share Menu Dialog  */
    @JvmField
    var shareMenuHeader = mDevice.findObject(
        UiSelector()
            .resourceId("android:id/title")
            .text("Share via")
            .enabled(true)
    )

    @JvmField
    var shareAppList = mDevice.findObject(
        UiSelector()
            .resourceId("android:id/resolver_list")
            .enabled(true)
    )

    @JvmStatic
    fun waitForIdle() {
        mDevice.waitForIdle(waitingTime)
    }

    @JvmStatic
    fun pressEnterKey() {
        mDevice.pressKeyCode(KeyEvent.KEYCODE_ENTER)
    }

    @JvmStatic
    fun pressBackKey() {
        mDevice.pressBack()
    }

    @JvmStatic
    fun pressHomeKey() {
        mDevice.pressHome()
    }

    @JvmStatic
    @Throws(IOException::class)
    fun createMockResponseFromAsset(fileName: String): MockResponse {
        return MockResponse()
            .setBody(readTestAsset(fileName))
    }

    @JvmStatic
    @Throws(IOException::class)
    fun readTestAsset(filename: String?): Buffer {
        InstrumentationRegistry.getInstrumentation().getContext().assets.open(filename!!)
            .use { stream -> return readStreamFile(stream) }
    }

    @Throws(IOException::class)
    fun readStreamFile(file: InputStream?): Buffer {
        val buffer = Buffer()
        buffer.write(file!!.readBytes())
        return buffer
    }

    @JvmStatic
    @Throws(IOException::class)
    fun readFileToString(file: File): String {
        println("Reading file: " + file.absolutePath)
        FileInputStream(file).use { stream -> return readStreamIntoString(stream) }
    }

    @Throws(IOException::class)
    fun readStreamIntoString(stream: InputStream?): String {
        BufferedReader(
            InputStreamReader(stream, StandardCharsets.UTF_8)
        ).use { reader ->
            val builder = StringBuilder()
            var line: String?
            while (reader.readLine().also { line = it } != null) {
                builder.append(line)
            }
            reader.close()
            return builder.toString()
        }
    }

    @JvmStatic
    fun waitForWebSiteTitleLoad() {
        Web.onWebView(ViewMatchers.withText("focus test page"))
    }

    @JvmStatic
    fun selectGeckoForKlar() {
        InstrumentationRegistry.getInstrumentation().getTargetContext().getSharedPreferences(
            "mozilla.components.service.fretboard.overrides",
            Context.MODE_PRIVATE
        )
            .edit()
            .putBoolean("use-gecko", isKlarBuild)
            .commit()
    }

    @Suppress("Deprecation")
    fun createCustomTabIntent(
        pageUrl: String,
        customMenuItemLabel: String = "",
        customActionButtonDescription: String = ""
    ): Intent {
        val appContext = InstrumentationRegistry.getInstrumentation()
            .targetContext
            .applicationContext
        val pendingIntent = PendingIntent.getActivity(appContext, 0, Intent(), IntentUtils.defaultIntentPendingFlags)
        val customTabsIntent = CustomTabsIntent.Builder()
            .addMenuItem(customMenuItemLabel, pendingIntent)
            .addDefaultShareMenuItem()
            .setActionButton(createTestBitmap(), customActionButtonDescription, pendingIntent, true)
            .setToolbarColor(Color.MAGENTA)
            .build()
        customTabsIntent.intent.data = Uri.parse(pageUrl)
        customTabsIntent.intent.component = ComponentName(appContext, IntentReceiverActivity::class.java)
        return customTabsIntent.intent
    }

    fun assertNativeAppOpens(appPackageName: String) {
        try {
            if (isPackageInstalled(packageName)) {
                intended(toPackage(appPackageName))
            }
        } catch (e: AssertionFailedError) {
            e.printStackTrace()
        }
    }

    private fun createTestBitmap(): Bitmap {
        val bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(bitmap)
        canvas.drawColor(Color.GREEN)
        return bitmap
    }
}
