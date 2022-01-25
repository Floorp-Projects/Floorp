/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import android.app.PendingIntent
import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.view.View
import androidx.browser.customtabs.CustomTabsIntent
import androidx.test.espresso.Espresso
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.IdlingRegistry
import androidx.test.espresso.action.ViewActions.longClick
import androidx.test.espresso.assertion.ViewAssertions
import androidx.test.espresso.intent.Intents
import androidx.test.espresso.intent.Intents.intended
import androidx.test.espresso.intent.matcher.IntentMatchers
import androidx.test.espresso.intent.matcher.IntentMatchers.toPackage
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.withChild
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withParent
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiObject
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import kotlinx.coroutines.runBlocking
import mozilla.components.support.ktx.android.content.appName
import org.hamcrest.CoreMatchers
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.Matcher
import org.junit.Assert
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.ext.waitNotNull
import org.mozilla.fenix.helpers.idlingresource.NetworkConnectionIdlingResource
import org.mozilla.fenix.ui.robots.BrowserRobot
import org.mozilla.fenix.ui.robots.mDevice
import org.mozilla.fenix.utils.IntentUtils
import java.io.File

object TestHelper {

    val appContext: Context = InstrumentationRegistry.getInstrumentation().targetContext
    val packageName: String = appContext.packageName
    val appName = appContext.appName

    fun scrollToElementByText(text: String): UiScrollable {
        val appView = UiScrollable(UiSelector().scrollable(true))
        appView.waitForExists(waitingTime)
        appView.scrollTextIntoView(text)
        return appView
    }

    fun longTapSelectItem(url: Uri) {
        mDevice.waitNotNull(
            Until.findObject(By.text(url.toString())),
            TestAssetHelper.waitingTime
        )
        onView(
            allOf(
                withId(R.id.url),
                withText(url.toString())
            )
        ).perform(longClick())
    }

    fun restartApp(activity: HomeActivityIntentTestRule) {
        with(activity) {
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
            TestAssetHelper.waitingTime
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
            `package` = "org.mozilla.fenix.debug"
            flags = Intent.FLAG_ACTIVITY_NEW_TASK
        }
        try {
            context.startActivity(intent)
        } catch (ex: ActivityNotFoundException) {
            intent.setPackage(null)
            context.startActivity(intent)
        }
    }

    // Remove test file from the device Downloads folder
    @Suppress("Deprecation")
    fun deleteDownloadFromStorage(fileName: String) {
        runBlocking {
            val downloadedFile = File(
                Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS),
                fileName
            )

            if (downloadedFile.exists()) {
                downloadedFile.delete()
            }
        }
    }

    fun setNetworkEnabled(enabled: Boolean) {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
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
        customActionButtonDescription: String = ""
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
                customActionButtonDescription, pendingIntent, true
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
            Intents.intended(IntentMatchers.toPackage(appPackageName))
        } else {
            val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
            mDevice.waitNotNull(
                Until.findObject(By.text("Could not open file")),
                waitingTime
            )
        }
    }

    fun assertNativeAppOpens(appPackageName: String, url: String) {
        if (isPackageInstalled(appPackageName)) {
            intended(toPackage(appPackageName))
        } else {
            BrowserRobot().verifyUrl(url)
        }
    }

    fun returnToBrowser() {
        val urlBar =
            mDevice.findObject(UiSelector().resourceId("$packageName:id/mozac_browser_toolbar_url_view"))
        do {
            mDevice.pressBack()
        } while (
            !urlBar.waitForExists(waitingTime)
        )
    }

    fun UiDevice.waitForObjects(obj: UiObject, waitingTime: Long = TestAssetHelper.waitingTime) {
        this.waitForIdle()
        Assert.assertNotNull(obj.waitForExists(waitingTime))
    }

    fun hasCousin(matcher: Matcher<View>): Matcher<View> {
        return withParent(
            hasSibling(
                withChild(
                    matcher
                )
            )
        )
    }
}
