/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.helpers

import android.annotation.TargetApi
import android.app.PendingIntent
import android.content.ActivityNotFoundException
import android.content.ComponentName
import android.content.ContentResolver
import android.content.ContentUris
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.net.Uri
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import android.os.Bundle
import android.provider.MediaStore
import android.provider.MediaStore.setIncludePending
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
import okio.Buffer
import org.hamcrest.Matchers
import org.hamcrest.Matchers.allOf
import org.junit.Assert
import org.junit.Assert.assertTrue
import org.mozilla.focus.R
import org.mozilla.focus.activity.IntentReceiverActivity
import org.mozilla.focus.utils.IntentUtils
import java.io.IOException
import java.io.InputStream

@Suppress("TooManyFunctions")
object TestHelper {
    @JvmField
    var mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
    const val waitingTime = DateUtils.SECOND_IN_MILLIS * 15
    const val pageLoadingTime = DateUtils.SECOND_IN_MILLIS * 25
    const val waitingTimeShort = DateUtils.SECOND_IN_MILLIS * 5

    private val charPool: List<Char> = ('a'..'z') + ('A'..'Z') + ('0'..'9')
    fun randomString(stringLength: Int) =
        (1..stringLength)
            .map { kotlin.random.Random.nextInt(0, charPool.size) }
            .map(charPool::get)
            .joinToString("")

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

    fun openAppFromExternalLink(url: String) {
        val intent = Intent().apply {
            action = Intent.ACTION_VIEW
            data = Uri.parse(url)
            `package` = packageName
            flags = Intent.FLAG_ACTIVITY_NEW_TASK
        }
        try {
            getTargetContext.startActivity(intent)
        } catch (ex: ActivityNotFoundException) {
            intent.setPackage(null)
            getTargetContext.startActivity(intent)
        }
    }

    fun verifyDownloadedFileOnStorage(fileName: String) {
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val resolver = context.contentResolver
        val fileUri = queryDownloadMediaStore(fileName)

        val fileExists: Boolean = fileUri?.let {
            resolver.openInputStream(fileUri).use {
                it != null
            }
        } ?: false

        assertTrue(fileExists)
    }

    /**
     * Check the "Downloads" public directory for [fileName] and returns an URI to it.
     * May be `null` if there is no file with that name in "Downloads".
     */
    @TargetApi(Build.VERSION_CODES.Q)
    private fun queryDownloadMediaStore(fileName: String): Uri? {
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val resolver = context.contentResolver
        val queryProjection = arrayOf(MediaStore.Downloads._ID)
        val querySelection = "${MediaStore.Downloads.DISPLAY_NAME} = ?"
        val querySelectionArgs = arrayOf(fileName)

        val queryBundle = Bundle().apply {
            putString(ContentResolver.QUERY_ARG_SQL_SELECTION, querySelection)
            putStringArray(ContentResolver.QUERY_ARG_SQL_SELECTION_ARGS, querySelectionArgs)
        }

        // Query if we have a pending download with the same name. This can happen
        // if a download was interrupted, failed or cancelled before the file was
        // written to disk. Our logic above will have generated a unique file name
        // based on existing files on the device, but we might already have a row
        // for the download in the content resolver.
        val collection = MediaStore.Downloads.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY)
        val queryCollection =
            if (SDK_INT >= Build.VERSION_CODES.R) {
                queryBundle.putInt(MediaStore.QUERY_ARG_MATCH_PENDING, MediaStore.MATCH_INCLUDE)
                collection
            } else {
                @Suppress("DEPRECATION")
                setIncludePending(collection)
            }

        var downloadUri: Uri? = null
        resolver.query(
            queryCollection,
            queryProjection,
            queryBundle,
            null
        )?.use {
            if (it.count > 0) {
                val idColumnIndex = it.getColumnIndex(MediaStore.Downloads._ID)
                it.moveToFirst()
                downloadUri = ContentUris.withAppendedId(collection, it.getLong(idColumnIndex))
            }
        }

        return downloadUri
    }

    // Method for granting app permission to access location/camera/mic
    fun grantAppPermission() {
        if (SDK_INT >= 23) {
            mDevice.findObject(
                UiSelector().textContains(
                    when (SDK_INT) {
                        Build.VERSION_CODES.R ->
                            "While using the app"
                        else -> "Allow"
                    }
                )
            ).click()
        }
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

    /********* Old code locators - used only in Screenshots tests  */
    // wait for web area to be visible
    @JvmStatic
    fun waitForWebContent() {
        Assert.assertTrue(geckoView.waitForExists(waitingTime))
    }

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
    var shareAppList = mDevice.findObject(
        UiSelector()
            .resourceId("android:id/resolver_list")
            .enabled(true)
    )

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
    fun waitForWebSiteTitleLoad() {
        Web.onWebView(ViewMatchers.withText("focus test page"))
    }
}
