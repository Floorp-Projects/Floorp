/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("DEPRECATION")

package org.mozilla.fenix.helpers

import android.Manifest
import android.app.ActivityManager
import android.app.PendingIntent
import android.content.ActivityNotFoundException
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.content.res.Configuration
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.net.Uri
import android.os.Build
import android.os.storage.StorageManager
import android.os.storage.StorageVolume
import android.provider.Settings
import android.util.Log
import android.view.View
import androidx.annotation.RequiresApi
import androidx.browser.customtabs.CustomTabsIntent
import androidx.test.espresso.Espresso
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.IdlingRegistry
import androidx.test.espresso.IdlingResource
import androidx.test.espresso.action.ViewActions.longClick
import androidx.test.espresso.assertion.ViewAssertions
import androidx.test.espresso.intent.Intents.intended
import androidx.test.espresso.intent.matcher.IntentMatchers.toPackage
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.withChild
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withParent
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.ActivityTestRule
import androidx.test.runner.permission.PermissionRequester
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiObject
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import junit.framework.AssertionFailedError
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.availableSearchEngines
import mozilla.components.support.ktx.android.content.appName
import org.hamcrest.CoreMatchers
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.Matcher
import org.junit.Assert
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.mozilla.fenix.Config
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.customtabs.ExternalAppBrowserActivity
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.Constants.PackageName.YOUTUBE_APP
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.ext.waitNotNull
import org.mozilla.fenix.helpers.idlingresource.NetworkConnectionIdlingResource
import org.mozilla.fenix.ui.robots.BrowserRobot
import org.mozilla.fenix.ui.robots.clickPageObject
import org.mozilla.fenix.utils.IntentUtils
import org.mozilla.gecko.util.ThreadUtils
import java.io.File
import java.util.Locale

object TestHelper {

    val appContext: Context = InstrumentationRegistry.getInstrumentation().targetContext
    val appName = appContext.appName
    var mDevice: UiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
    val packageName: String = appContext.packageName

    fun scrollToElementByText(text: String): UiScrollable {
        val appView = UiScrollable(UiSelector().scrollable(true))
        appView.waitForExists(waitingTime)
        appView.scrollTextIntoView(text)
        return appView
    }

    fun longTapSelectItem(url: Uri) {
        mDevice.waitNotNull(
            Until.findObject(By.text(url.toString())),
            waitingTime,
        )
        onView(
            allOf(
                withId(R.id.url),
                withText(url.toString()),
            ),
        ).perform(longClick())
    }

    fun restartApp(activity: HomeActivityIntentTestRule) {
        with(activity) {
            updateCachedSettings()
            finishActivity()
            mDevice.waitForIdle()
            launchActivity(null)
        }
    }

    fun getPermissionAllowID(): String {
        return when
            (Build.VERSION.SDK_INT > Build.VERSION_CODES.P) {
            true -> "com.android.permissioncontroller"
            false -> "com.android.packageinstaller"
        }
    }

    fun waitUntilObjectIsFound(resourceName: String) {
        mDevice.waitNotNull(
            Until.findObjects(By.res(resourceName)),
            waitingTime,
        )
    }

    fun clickSnackbarButton(expectedText: String) =
        clickPageObject(itemWithResIdAndText("$packageName:id/snackbar_btn", expectedText))

    fun waitUntilSnackbarGone() {
        mDevice.findObject(
            UiSelector().resourceId("$packageName:id/snackbar_layout"),
        ).waitUntilGone(waitingTime)
    }

    fun verifySnackBarText(expectedText: String) {
        assertTrue(
            mDevice.findObject(
                UiSelector()
                    .textContains(expectedText),
            ).waitForExists(waitingTime),
        )
    }

    fun verifyUrl(urlSubstring: String, resourceName: String, resId: Int) {
        waitUntilObjectIsFound(resourceName)
        mDevice.findObject(UiSelector().text(urlSubstring)).waitForExists(waitingTime)
        onView(withId(resId)).check(ViewAssertions.matches(withText(CoreMatchers.containsString(urlSubstring))))
    }

    fun openAppFromExternalLink(url: String) {
        val context = InstrumentationRegistry.getInstrumentation().getTargetContext()
        val intent = Intent().apply {
            action = Intent.ACTION_VIEW
            data = Uri.parse(url)
            `package` = packageName
            flags = Intent.FLAG_ACTIVITY_NEW_TASK
        }
        try {
            context.startActivity(intent)
        } catch (ex: ActivityNotFoundException) {
            intent.setPackage(null)
            context.startActivity(intent)
        }
    }

    @RequiresApi(Build.VERSION_CODES.R)
    fun deleteDownloadedFileOnStorage(fileName: String) {
        val storageManager: StorageManager? = appContext.getSystemService(Context.STORAGE_SERVICE) as StorageManager?
        val storageVolumes = storageManager!!.storageVolumes
        val storageVolume: StorageVolume = storageVolumes[0]
        val file = File(storageVolume.directory!!.path + "/Download/" + fileName)
        try {
            file.delete()
            Log.d("TestLog", "File delete try 1")
            assertFalse("The file was not deleted", file.exists())
        } catch (e: AssertionError) {
            file.delete()
            Log.d("TestLog", "File delete retried")
            assertFalse("The file was not deleted", file.exists())
        }
    }

    fun setNetworkEnabled(enabled: Boolean) {
        val networkDisconnectedIdlingResource = NetworkConnectionIdlingResource(false)
        val networkConnectedIdlingResource = NetworkConnectionIdlingResource(true)

        when (enabled) {
            true -> {
                mDevice.executeShellCommand("svc data enable")
                mDevice.executeShellCommand("svc wifi enable")

                // Wait for network connection to be completely enabled
                IdlingRegistry.getInstance().register(networkConnectedIdlingResource)
                Espresso.onIdle {
                    IdlingRegistry.getInstance().unregister(networkConnectedIdlingResource)
                }
            }

            false -> {
                mDevice.executeShellCommand("svc data disable")
                mDevice.executeShellCommand("svc wifi disable")

                // Wait for network connection to be completely disabled
                IdlingRegistry.getInstance().register(networkDisconnectedIdlingResource)
                Espresso.onIdle {
                    IdlingRegistry.getInstance().unregister(networkDisconnectedIdlingResource)
                }
            }
        }
    }

    fun createCustomTabIntent(
        pageUrl: String,
        customMenuItemLabel: String = "",
        customActionButtonDescription: String = "",
    ): Intent {
        val appContext = InstrumentationRegistry.getInstrumentation()
            .targetContext
            .applicationContext
        val pendingIntent = PendingIntent.getActivity(appContext, 0, Intent(), IntentUtils.defaultIntentPendingFlags)
        val customTabsIntent = CustomTabsIntent.Builder()
            .addMenuItem(customMenuItemLabel, pendingIntent)
            .setShareState(CustomTabsIntent.SHARE_STATE_ON)
            .setActionButton(
                createTestBitmap(),
                customActionButtonDescription,
                pendingIntent,
                true,
            )
            .build()
        customTabsIntent.intent.data = Uri.parse(pageUrl)
        return customTabsIntent.intent
    }

    private fun createTestBitmap(): Bitmap {
        val bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(bitmap)
        canvas.drawColor(Color.GREEN)
        return bitmap
    }

    fun isPackageInstalled(packageName: String): Boolean {
        return try {
            val packageManager = InstrumentationRegistry.getInstrumentation().context.packageManager
            packageManager.getApplicationInfo(packageName, 0).enabled
        } catch (exception: PackageManager.NameNotFoundException) {
            false
        }
    }

    fun assertExternalAppOpens(appPackageName: String) {
        if (isPackageInstalled(appPackageName)) {
            try {
                intended(toPackage(appPackageName))
            } catch (e: AssertionFailedError) {
                e.printStackTrace()
            }
        } else {
            mDevice.waitNotNull(
                Until.findObject(By.text("Could not open file")),
                waitingTime,
            )
        }
    }

    fun assertNativeAppOpens(appPackageName: String, url: String = "") {
        if (isPackageInstalled(appPackageName)) {
            mDevice.waitForIdle(waitingTimeShort)
            assertTrue(
                mDevice.findObject(UiSelector().packageName(appPackageName))
                    .waitForExists(waitingTime),
            )
        } else {
            BrowserRobot().verifyUrl(url)
        }
    }

    fun assertYoutubeAppOpens() = intended(toPackage(YOUTUBE_APP))

    /**
     * Checks whether the latest activity of the application is used for custom tabs or PWAs.
     *
     * @return Boolean value that helps us know if the current activity supports custom tabs or PWAs.
     */
    fun isExternalAppBrowserActivityInCurrentTask(): Boolean {
        val activityManager = appContext.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager

        mDevice.waitForIdle(waitingTimeShort)

        return activityManager.appTasks[0].taskInfo.topActivity!!.className == ExternalAppBrowserActivity::class.java.name
    }

    /**
     * Run test with automatically registering idling resources and cleanup.
     *
     * @param idlingResources zero or more [IdlingResource] to be used when running [testBlock].
     * @param testBlock test code to execute.
     */
    fun registerAndCleanupIdlingResources(
        vararg idlingResources: IdlingResource,
        testBlock: () -> Unit,
    ) {
        idlingResources.forEach {
            IdlingRegistry.getInstance().register(it)
        }

        try {
            testBlock()
        } finally {
            idlingResources.forEach {
                IdlingRegistry.getInstance().unregister(it)
            }
        }
    }

    // exit from Menus to home screen or browser
    fun exitMenu() {
        val toolbar =
            mDevice.findObject(UiSelector().resourceId("$packageName:id/toolbar"))
        while (!toolbar.waitForExists(waitingTimeShort)) {
            mDevice.pressBack()
        }
    }

    fun UiDevice.waitForObjects(obj: UiObject, waitingTime: Long = TestAssetHelper.waitingTime) {
        this.waitForIdle()
        Assert.assertNotNull(obj.waitForExists(waitingTime))
    }

    fun hasCousin(matcher: Matcher<View>): Matcher<View> {
        return withParent(
            hasSibling(
                withChild(
                    matcher,
                ),
            ),
        )
    }

    fun getStringResource(id: Int, argument: String = appName) = appContext.resources.getString(id, argument)

    // Permission allow dialogs differ on various Android APIs
    fun grantSystemPermission() {
        val whileUsingTheAppPermissionButton: UiObject =
            mDevice.findObject(UiSelector().textContains("While using the app"))

        val allowPermissionButton: UiObject =
            mDevice.findObject(
                UiSelector()
                    .textContains("Allow")
                    .className("android.widget.Button"),
            )

        if (Build.VERSION.SDK_INT >= 23) {
            if (whileUsingTheAppPermissionButton.waitForExists(waitingTimeShort)) {
                whileUsingTheAppPermissionButton.click()
            } else if (allowPermissionButton.waitForExists(waitingTimeShort)) {
                allowPermissionButton.click()
            }
        }
    }

    // Permission deny dialogs differ on various Android APIs
    fun denyPermission() {
        mDevice.findObject(UiSelector().textContains("Deny")).waitForExists(waitingTime)
        mDevice.findObject(UiSelector().textContains("Deny")).click()
    }

    fun isTestLab(): Boolean {
        return Settings.System.getString(appContext.contentResolver, "firebase.test.lab").toBoolean()
    }

    private val charPool: List<Char> = ('a'..'z') + ('A'..'Z') + ('0'..'9')
    fun generateRandomString(stringLength: Int) =
        (1..stringLength)
            .map { kotlin.random.Random.nextInt(0, charPool.size) }
            .map(charPool::get)
            .joinToString("")

    /**
     * Changes the default language of the entire device, not just the app.
     * Runs on Debug variant as we don't want to adjust Release permission manifests
     * Runs the test in its testBlock.
     * Cleans up and sets the default locale after it's done.
     */
    fun runWithSystemLocaleChanged(locale: Locale, testRule: ActivityTestRule<HomeActivity>, testBlock: () -> Unit) {
        if (Config.channel.isDebug) {
            /* Sets permission to change device language */
            PermissionRequester().apply {
                addPermissions(
                    Manifest.permission.CHANGE_CONFIGURATION,
                )
                requestPermissions()
            }

            val defaultLocale = Locale.getDefault()

            try {
                setSystemLocale(locale)
                testBlock()
                ThreadUtils.runOnUiThread { testRule.activity.recreate() }
            } catch (e: Exception) {
                e.printStackTrace()
            } finally {
                setSystemLocale(defaultLocale)
            }
        }
    }

    /**
     * Changes the default language of the entire device, not just the app.
     */
    fun setSystemLocale(locale: Locale) {
        val activityManagerNative = Class.forName("android.app.ActivityManagerNative")
        val am = activityManagerNative.getMethod("getDefault", *arrayOfNulls(0))
            .invoke(activityManagerNative, *arrayOfNulls(0))
        val config = InstrumentationRegistry.getInstrumentation().context.resources.configuration
        config.javaClass.getDeclaredField("locale")[config] = locale
        config.javaClass.getDeclaredField("userSetLocale").setBoolean(config, true)
        am.javaClass.getMethod(
            "updateConfiguration",
            Configuration::class.java,
        ).invoke(am, config)
    }

    /**
     * Creates clipboard data.
     */
    fun setTextToClipBoard(context: Context, message: String) {
        val clipBoard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        val clipData = ClipData.newPlainText("label", message)

        clipBoard.setPrimaryClip(clipData)
    }

    /**
     * Returns sponsored shortcut title based on the index.
     */
    fun getSponsoredShortcutTitle(position: Int): String {
        val sponsoredShortcut = mDevice.findObject(
            UiSelector()
                .resourceId("$packageName:id/top_site_item")
                .index(position - 1),
        ).getChild(
            UiSelector()
                .resourceId("$packageName:id/top_site_title"),
        ).text

        return sponsoredShortcut
    }

    fun verifyLightThemeApplied(expected: Boolean) =
        assertFalse("Light theme not selected", expected)

    fun verifyDarkThemeApplied(expected: Boolean) = assertTrue("Dark theme not selected", expected)

    /**
     * Wrapper for tests to run only when certain conditions are met.
     * For example: this method will avoid accidentally running a test on GV versions where the feature is disabled.
     */
    fun runWithCondition(condition: Boolean, testBlock: () -> Unit) {
        if (condition) {
            testBlock()
        }
    }

    fun putAppToBackground() {
        mDevice.pressRecentApps()
        mDevice.findObject(UiSelector().resourceId("$packageName:id/container")).waitUntilGone(waitingTime)
    }

    fun bringAppToForeground() {
        mDevice.pressRecentApps()
        mDevice.findObject(UiSelector().resourceId("$packageName:id/container")).waitForExists(waitingTime)
    }

    fun verifyKeyboardVisibility(isExpectedToBeVisible: Boolean = true) {
        mDevice.waitForIdle()

        assertEquals(
            "Keyboard not shown",
            isExpectedToBeVisible,
            mDevice
                .executeShellCommand("dumpsys input_method | grep mInputShown")
                .contains("mInputShown=true"),
        )
    }

    /**
     * The list of Search engines for the "home" region of the user.
     * For en-us it will return the 6 engines selected by default: Google, Bing, DuckDuckGo, Amazon, Ebay, Wikipedia.
     */
    fun getRegionSearchEnginesList(): List<SearchEngine> {
        val searchEnginesList = appContext.components.core.store.state.search.regionSearchEngines
        assertTrue("Search engines list returned nothing", searchEnginesList.isNotEmpty())
        return searchEnginesList
    }

    /**
     * The list of Search engines available to be added by user choice.
     * For en-us it will return the 2 engines: Reddit, Youtube.
     */
    fun getAvailableSearchEngines(): List<SearchEngine> {
        val searchEnginesList = appContext.components.core.store.state.search.availableSearchEngines
        assertTrue("Search engines list returned nothing", searchEnginesList.isNotEmpty())
        return searchEnginesList
    }
}
