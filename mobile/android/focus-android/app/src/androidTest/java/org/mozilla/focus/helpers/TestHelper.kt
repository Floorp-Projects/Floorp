/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.helpers

import android.content.Context
import android.content.pm.PackageManager
import android.content.res.Resources
import android.text.format.DateUtils
import android.util.DisplayMetrics
import android.util.Log
import android.view.KeyEvent
import androidx.test.espresso.Espresso
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.web.sugar.Web
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiObject
import androidx.test.uiautomator.UiObjectNotFoundException
import androidx.test.uiautomator.UiSelector
import okhttp3.mockwebserver.MockResponse
import okio.Buffer
import okio.Okio
import org.hamcrest.Matchers
import org.junit.Assert
import org.junit.Assert.assertTrue
import org.mozilla.focus.R
import org.mozilla.focus.utils.AppConstants.isKlarBuild
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
    const val waitingTime = DateUtils.SECOND_IN_MILLIS * 4
    const val webPageLoadwaitingTime = DateUtils.SECOND_IN_MILLIS * 15

    @JvmStatic
    val appName: String
        get() = InstrumentationRegistry.getInstrumentation()
            .targetContext.packageName

    @JvmStatic
    val appContext = InstrumentationRegistry.getInstrumentation().targetContext.applicationContext

    fun getStringResource(id: Int) = appContext.getString(id)

    fun verifySnackBarText(text: String) {
        val snackbarText = mDevice.findObject(
            UiSelector().resourceId("$appName:id/snackbar_text")
        )
        snackbarText.waitForExists(waitingTime)
        assertTrue(snackbarText.text.contains(text))
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

    // wait for web area to be visible
    @JvmStatic
    fun waitForWebContent() {
        Assert.assertTrue(geckoView.waitForExists(waitingTime))
    }

    /********* First View Locators  */
    @JvmField
    var firstSlide = mDevice.findObject(
        UiSelector()
            .text("Power up your privacy")
            .enabled(true)
    )

    @JvmField
    var secondSlide = mDevice.findObject(
        UiSelector()
            .text("Your search, your way")
            .enabled(true)
    )

    @JvmField
    var thirdSlide = mDevice.findObject(
        UiSelector()
            .text("Add shortcuts to your home screen")
            .enabled(true)
    )

    @JvmField
    var lastSlide = mDevice.findObject(
        UiSelector()
            .text("Make privacy a habit")
            .enabled(true)
    )

    @JvmField
    var nextBtn = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/next")
            .enabled(true)
    )

    @JvmField
    var finishBtn = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/finish")
            .enabled(true)
    )

    @JvmField
    var initialView = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/backgroundView")
            .enabled(true)
    )

    /********* Main View Locators  */
    @JvmField
    var menuButton = Espresso.onView(
        Matchers.allOf(
            ViewMatchers.withId(R.id.menuView),
            ViewMatchers.isDisplayed()
        )
    )

    /********* Web View Locators  */
    @JvmField
    var browserURLbar = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/display_url")
            .clickable(true)
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
            .resourceId(appName + ":id/urlView")
            .focused(true)
            .enabled(true)
    )

    @JvmField
    var searchSuggestionsTitle = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/enable_search_suggestions_title")
            .enabled(true)
    )

    @JvmField
    var searchSuggestionsButtonYes = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/enable_search_suggestions_button")
            .enabled(true)
    )

    @JvmField
    var cleartextField = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/clearView")
            .enabled(true)
    )

    @JvmField
    var hint = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/searchView")
            .clickable(true)
    )

    @JvmField
    var suggestionList = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/suggestionList")
    )
    var suggestion = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/suggestion")
    )

    @JvmField
    var webView = mDevice.findObject(
        UiSelector()
            .className("android.webkit.WebView")
            .enabled(true)
    )
    var geckoView = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/webview")
            .enabled(true)
    )

    @JvmField
    var progressBar = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/progress")
            .enabled(true)
    )
    var tryAgainBtn = mDevice.findObject(
        UiSelector()
            .resourceId("errorTryAgain")
            .clickable(true)
    )

    @JvmField
    var floatingEraseButton = Espresso.onView(
        Matchers.allOf(ViewMatchers.withId(R.id.erase), ViewMatchers.isDisplayed())
    )
    var browserViewSettingsMenuItem = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/settings")
            .clickable(true)
    )

    @JvmField
    var erasedMsg = mDevice.findObject(
        UiSelector()
            .text("Your browsing history has been erased.")
            .resourceId(appName + ":id/snackbar_text")
            .enabled(true)
    )

    @JvmField
    var lockIcon = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/lock")
            .description("Secure connection")
    )

    @JvmField
    var notificationBarDeleteItem = mDevice.findObject(
        UiSelector()
            .text("Erase browsing history")
            .resourceId("android:id/text")
            .enabled(true)
    )
    var notificationExpandSwitch = mDevice.findObject(
        UiSelector()
            .resourceId("android:id/expand_button")
            .packageName(appName + "")
            .enabled(true)
    )

    @JvmField
    var notificationOpenItem = mDevice.findObject(
        UiSelector()
            .resourceId("android:id/action0")
            .description("Open")
            .enabled(true)
    )

    @JvmField
    var notificationEraseOpenItem = mDevice.findObject(
        UiSelector()
            .resourceId("android:id/action0")
            .description("Erase and Open")
            .enabled(true)
    )
    var blockOffIcon = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/block")
            .enabled(true)
    )

    @JvmField
    var AddtoHSmenuItem = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/add_to_homescreen")
            .enabled(true)
    )

    @JvmField
    var AddtoHSCancelBtn = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/addtohomescreen_dialog_cancel")
            .enabled(true)
    )

    @JvmField
    var AddtoHSOKBtn = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/addtohomescreen_dialog_add")
            .enabled(true)
    )

    @JvmField
    var AddautoBtn = mDevice.findObject(
        UiSelector()
            .className("android.widget.Button")
            .instance(1)
            .enabled(true)
    )

    @JvmField
    var shortcutTitle = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/edit_title")
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
            .resourceId(appName + ":id/security_info")
            .enabled(true)
    )

    @JvmField
    var identityState = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/site_identity_state")
            .enabled(true)
    )

    @JvmField
    var downloadTitle = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/title_template")
            .enabled(true)
    )

    @JvmField
    var downloadBtn = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/download_dialog_download")
            .enabled(true)
    )

    /********* Main View Menu Item Locators  */
    var whatsNewItem = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/whats_new")
            .enabled(true)
    )

    @JvmField
    var HelpItem = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/help")
            .enabled(true)
    )

    @JvmField
    var settingsMenuItem = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/settings")
            .enabled(true)
    )

    @JvmField
    var blockCounterItem = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/trackers_count")
    )

    @JvmField
    var menulist = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/list")
            .enabled(true)
    )

    @Throws(UiObjectNotFoundException::class)
    fun getMenuItemText(item: UiObject): String {
        return item.getChild(UiSelector().index(0))
            .getChild(UiSelector().index(0)).text
    }

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

    /********* Settings Menu Item Locators  */
    @JvmField
    var settingsMenu = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/recycler_view")
    )

    @JvmField
    var settingsHeading = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/toolbar")
            .enabled(true)
    )
    var navigateUp = mDevice.findObject(
        UiSelector()
            .description("Navigate up")
    )
    var toggleAnalyticBlock = mDevice.findObject(
        UiSelector()
            .className("android.widget.Switch")
            .instance(1)
    )
    var refreshBtn = mDevice.findObject(
        UiSelector()
            .resourceId(appName + ":id/refresh")
            .enabled(true)
    )

    const val PINCH_PERCENT = 50
    const val PINCH_STEPS = 5

    @JvmStatic
    @Throws(UiObjectNotFoundException::class)
    fun expandNotification() {
        if (!notificationOpenItem.waitForExists(waitingTime)) {
            if (!notificationExpandSwitch.exists()) {
                notificationBarDeleteItem.pinchOut(PINCH_PERCENT, PINCH_STEPS)
            } else {
                notificationExpandSwitch.click()
            }
            Assert.assertTrue(notificationOpenItem.exists())
        }
    }

    const val X_OFFSET = 20
    const val Y_OFFSET = 500
    const val STEPS = 10
    private fun devicePixels(): DisplayMetrics {
        return Resources.getSystem().displayMetrics
    }

    @JvmStatic
    fun swipeScreenLeft() {
        val metrics = devicePixels()
        mDevice.swipe(metrics.widthPixels - X_OFFSET, Y_OFFSET, 0, Y_OFFSET, STEPS)
    }

    fun swipeScreenRight() {
        val metrics = devicePixels()
        mDevice.swipe(X_OFFSET, Y_OFFSET, metrics.widthPixels, Y_OFFSET, STEPS)
    }

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
    fun openNotification() {
        mDevice.openNotification()
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
        buffer.writeAll(Okio.source(file!!))
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
}
