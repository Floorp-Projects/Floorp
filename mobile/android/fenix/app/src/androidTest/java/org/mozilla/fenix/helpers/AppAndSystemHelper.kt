/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
@file:Suppress("DEPRECATION")

package org.mozilla.fenix.helpers

import android.Manifest
import android.app.ActivityManager
import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.content.res.Configuration
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.os.storage.StorageManager
import android.os.storage.StorageVolume
import android.provider.Settings
import android.util.Log
import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.test.espresso.Espresso
import androidx.test.espresso.IdlingRegistry
import androidx.test.espresso.IdlingResource
import androidx.test.espresso.intent.Intents.intended
import androidx.test.espresso.intent.matcher.IntentMatchers.toPackage
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.ActivityTestRule
import androidx.test.runner.permission.PermissionRequester
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiObject
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import junit.framework.AssertionFailedError
import kotlinx.coroutines.runBlocking
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.storage.sync.PlacesBookmarksStorage
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import org.junit.Assert
import org.junit.Assert.assertEquals
import org.mozilla.fenix.Config
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.components.PermissionStorage
import org.mozilla.fenix.customtabs.ExternalAppBrowserActivity
import org.mozilla.fenix.helpers.Constants.PackageName.PIXEL_LAUNCHER
import org.mozilla.fenix.helpers.Constants.PackageName.YOUTUBE_APP
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.TestHelper.appContext
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.ext.waitNotNull
import org.mozilla.fenix.helpers.idlingresource.NetworkConnectionIdlingResource
import org.mozilla.fenix.ui.robots.BrowserRobot
import org.mozilla.gecko.util.ThreadUtils
import java.io.File
import java.util.Locale

object AppAndSystemHelper {

    fun getPermissionAllowID(): String {
        return when
            (Build.VERSION.SDK_INT > Build.VERSION_CODES.P) {
            true -> "com.android.permissioncontroller"
            false -> "com.android.packageinstaller"
        }
    }

    /**
     * Checks if a specific download file is inside the device storage and deletes it.
     * Different implementation needed for newer API levels,
     * as Environment.getExternalStorageDirectory() is deprecated starting with API 29.
     *
     */
    fun deleteDownloadedFileOnStorage(fileName: String) {
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.Q) {
            val storageManager: StorageManager? =
                appContext.getSystemService(Context.STORAGE_SERVICE) as StorageManager?
            val storageVolumes = storageManager!!.storageVolumes
            val storageVolume: StorageVolume = storageVolumes[0]
            val file = File(storageVolume.directory!!.path + "/Download/" + fileName)
            try {
                if (file.exists()) {
                    file.delete()
                    Log.d("TestLog", "File delete try 1")
                    Assert.assertFalse("The file was not deleted", file.exists())
                }
            } catch (e: AssertionError) {
                file.delete()
                Log.d("TestLog", "File delete retried")
                Assert.assertFalse("The file was not deleted", file.exists())
            }
        } else {
            runBlocking {
                val downloadedFile = File(
                    Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS),
                    fileName,
                )

                if (downloadedFile.exists()) {
                    Log.i(TAG, "deleteDownloadedFileOnStorage: Verifying if $downloadedFile exists.")
                    downloadedFile.delete()
                    Log.i(TAG, "deleteDownloadedFileOnStorage: $downloadedFile deleted.")
                }
            }
        }
    }

    /**
     * Checks if there are download files inside the device storage and deletes all of them.
     * Different implementation needed for newer API levels, as
     * Environment.getExternalStorageDirectory() is deprecated starting with API 29.
     */
    fun clearDownloadsFolder() {
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.Q) {
            Log.i(TAG, "clearDownloadsFolder: API > 29")
            val storageManager: StorageManager? =
                appContext.getSystemService(Context.STORAGE_SERVICE) as StorageManager?
            val storageVolumes = storageManager!!.storageVolumes
            val storageVolume: StorageVolume = storageVolumes[0]
            val downloadsFolder = File(storageVolume.directory!!.path + "/Download/")

            // Check if the downloads folder exists
            if (downloadsFolder.exists() && downloadsFolder.isDirectory) {
                Log.i(TAG, "clearDownloadsFolder: Verified that \"DOWNLOADS\" folder exists")
                val files = downloadsFolder.listFiles()

                // Check if the folder is not empty
                // If you run this method before a test, files.isNotEmpty() will always return false.
                if (files != null && files.isNotEmpty()) {
                    Log.i(
                        TAG,
                        "clearDownloadsFolder: Before cleanup: Downloads storage contains: ${files.size} file(s)",
                    )
                    // Delete all files in the folder
                    for (file in files) {
                        file.delete()
                        Log.i(
                            TAG,
                            "clearDownloadsFolder: Deleted $file from \"DOWNLOADS\" folder." +
                                " Downloads storage contains ${files.size} file(s): $file",
                        )
                    }
                } else {
                    Log.i(
                        TAG,
                        "clearDownloadsFolder: Downloads storage is empty.",
                    )
                }
            }
        } else {
            runBlocking {
                Log.i(TAG, "clearDownloadsFolder: API <= 29")
                Log.i(TAG, "clearDownloadsFolder: Verifying if any download files exist.")
                Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS)
                    .listFiles()?.forEach {
                        Log.i(TAG, "clearDownloadsFolder: Downloads storage contains: $it.")
                        it.delete()
                        Log.i(TAG, "clearDownloadsFolder: Download file $it deleted.")
                    }
            }
        }
    }

    suspend fun deleteHistoryStorage() {
        val historyStorage = PlacesHistoryStorage(appContext.applicationContext)
        Log.i(
            TAG,
            "deleteHistoryStorage before cleanup: History storage contains: ${historyStorage.getVisited()}",
        )
        if (historyStorage.getVisited().isNotEmpty()) {
            Log.i(TAG, "deleteHistoryStorage: Trying to delete all history storage.")
            historyStorage.deleteEverything()
            Log.i(
                TAG,
                "deleteHistoryStorage after cleanup: History storage contains: ${historyStorage.getVisited()}",
            )
        }
    }

    suspend fun deleteBookmarksStorage() {
        val bookmarksStorage = PlacesBookmarksStorage(appContext.applicationContext)
        val bookmarks = bookmarksStorage.getTree(BookmarkRoot.Mobile.id)?.children
        Log.i(TAG, "deleteBookmarksStorage before cleanup: Bookmarks storage contains: $bookmarks")
        if (bookmarks?.isNotEmpty() == true) {
            bookmarks.forEach {
                Log.i(
                    TAG,
                    "deleteBookmarksStorage: Trying to delete $it bookmark from storage.",
                )
                bookmarksStorage.deleteNode(it.guid)
                // TODO: Follow-up with a method to handle the DB update; the logs will still show the bookmarks in the storage before the test starts.
                Log.i(
                    TAG,
                    "deleteBookmarksStorage: Bookmark deleted. Bookmarks storage contains: $bookmarks",
                )
            }
        }
    }

    suspend fun deletePermissionsStorage() {
        val permissionStorage = PermissionStorage(appContext.applicationContext)
        Log.i(
            TAG,
            "deletePermissionsStorage: Trying to delete permissions. Permissions storage contains: ${permissionStorage.getSitePermissionsPaged()}",
        )
        permissionStorage.deleteAllSitePermissions()
        Log.i(
            TAG,
            "deletePermissionsStorage: Permissions deleted. Permissions storage contains: ${permissionStorage.getSitePermissionsPaged()}",
        )
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
                Log.i(TAG, "setNetworkEnabled: Network connection was enabled")
            }

            false -> {
                mDevice.executeShellCommand("svc data disable")
                mDevice.executeShellCommand("svc wifi disable")

                // Wait for network connection to be completely disabled
                IdlingRegistry.getInstance().register(networkDisconnectedIdlingResource)
                Espresso.onIdle {
                    IdlingRegistry.getInstance().unregister(networkDisconnectedIdlingResource)
                }
                Log.i(TAG, "setNetworkEnabled: Network connection was disabled")
            }
        }
    }

    fun isPackageInstalled(packageName: String): Boolean {
        Log.i(TAG, "isPackageInstalled: Trying to verify that $packageName is installed")
        return try {
            val packageManager = InstrumentationRegistry.getInstrumentation().context.packageManager
            packageManager.getApplicationInfo(packageName, 0).enabled
        } catch (e: PackageManager.NameNotFoundException) {
            Log.i(TAG, "isPackageInstalled: $packageName is not installed - ${e.message}")
            false
        }
    }

    fun assertExternalAppOpens(appPackageName: String) {
        if (isPackageInstalled(appPackageName)) {
            Log.i(TAG, "assertExternalAppOpens: $appPackageName is installed on device")
            try {
                Log.i(TAG, "assertExternalAppOpens: Try block")
                intended(toPackage(appPackageName))
                Log.i(TAG, "assertExternalAppOpens: Matched intent to $appPackageName")
            } catch (e: AssertionFailedError) {
                Log.i(TAG, "assertExternalAppOpens: Catch block - ${e.message}")
            }
        } else {
            mDevice.waitNotNull(
                Until.findObject(By.text("Could not open file")),
                TestAssetHelper.waitingTime,
            )
            Log.i(TAG, "assertExternalAppOpens: Verified \"Could not open file\" message")
        }
    }

    fun assertNativeAppOpens(appPackageName: String, url: String = "") {
        if (isPackageInstalled(appPackageName)) {
            mDevice.waitForIdle(TestAssetHelper.waitingTimeShort)
            Assert.assertTrue(
                TestHelper.mDevice.findObject(UiSelector().packageName(appPackageName))
                    .waitForExists(TestAssetHelper.waitingTime),
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
        Log.i(TAG, "Trying to verify that the latest activity of the application is used for custom tabs or PWAs")
        val activityManager = appContext.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager

        mDevice.waitForIdle(TestAssetHelper.waitingTimeShort)

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
            if (whileUsingTheAppPermissionButton.waitForExists(TestAssetHelper.waitingTimeShort)) {
                whileUsingTheAppPermissionButton.click()
            } else if (allowPermissionButton.waitForExists(TestAssetHelper.waitingTimeShort)) {
                allowPermissionButton.click()
            }
        }
    }

    // Permission deny dialogs differ on various Android APIs
    fun denyPermission() {
        mDevice.findObject(UiSelector().textContains("Deny")).waitForExists(TestAssetHelper.waitingTime)
        mDevice.findObject(UiSelector().textContains("Deny")).click()
    }

    fun isTestLab(): Boolean {
        return Settings.System.getString(TestHelper.appContext.contentResolver, "firebase.test.lab").toBoolean()
    }

    /**
     * Changes the default language of the entire device, not just the app.
     * Runs the test in its testBlock.
     * Cleans up and sets the default locale after it's done.
     */
    fun runWithSystemLocaleChanged(locale: Locale, testRule: ActivityTestRule<HomeActivity>, testBlock: () -> Unit) {
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

    /**
     * Changes the default language of the entire device, not just the app.
     * We can only use this if we're running on a debug build, otherwise it will change the permission manifests in release builds.
     */
    fun setSystemLocale(locale: Locale) {
        if (Config.channel.isDebug) {
            /* Sets permission to change device language */
            Log.i(
                TAG,
                "setSystemLocale: Requesting permission to change system locale to $locale.",
            )
            PermissionRequester().apply {
                addPermissions(
                    Manifest.permission.CHANGE_CONFIGURATION,
                )
                requestPermissions()
            }
            Log.i(
                TAG,
                "setSystemLocale: Received permission to change system locale to $locale.",
            )
            val activityManagerNative = Class.forName("android.app.ActivityManagerNative")
            val am = activityManagerNative.getMethod("getDefault", *arrayOfNulls(0))
                .invoke(activityManagerNative, *arrayOfNulls(0))
            val config =
                InstrumentationRegistry.getInstrumentation().context.resources.configuration
            config.javaClass.getDeclaredField("locale")[config] = locale
            config.javaClass.getDeclaredField("userSetLocale").setBoolean(config, true)
            am.javaClass.getMethod(
                "updateConfiguration",
                Configuration::class.java,
            ).invoke(am, config)
        }
        Log.i(
            TAG,
            "setSystemLocale: Changed system locale to $locale.",
        )
    }

    fun putAppToBackground() {
        mDevice.pressRecentApps()
        mDevice.findObject(UiSelector().resourceId("${TestHelper.packageName}:id/container")).waitUntilGone(
            TestAssetHelper.waitingTime,
        )
    }

    /**
     * Brings the app to foregorund by clicking it in the recent apps tray.
     * The package name is related to the home screen experience for the Pixel phones produced by Google.
     * The recent apps tray on API 30 will always display only 2 apps, even if previously were opened more.
     * The index of the most recent opened app will always have index 2, meaning that the previously opened app will have index 1.
     */
    fun bringAppToForeground() =
        mDevice.findObject(UiSelector().index(2).packageName(PIXEL_LAUNCHER)).clickAndWaitForNewWindow(waitingTimeShort)

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

    fun openAppFromExternalLink(url: String) {
        val context = InstrumentationRegistry.getInstrumentation().getTargetContext()
        val intent = Intent().apply {
            action = Intent.ACTION_VIEW
            data = Uri.parse(url)
            `package` = TestHelper.packageName
            flags = Intent.FLAG_ACTIVITY_NEW_TASK
        }
        try {
            context.startActivity(intent)
        } catch (ex: ActivityNotFoundException) {
            intent.setPackage(null)
            context.startActivity(intent)
        }
    }

    /**
     * Wrapper for tests to run only when certain conditions are met.
     * For example: this method will avoid accidentally running a test on GV versions where the feature is disabled.
     */
    fun runWithCondition(condition: Boolean, testBlock: () -> Unit) {
        if (condition) {
            testBlock()
        }
    }

    /**
     * Wrapper to launch the app using the launcher intent.
     */
    fun runWithLauncherIntent(
        activityTestRule: AndroidComposeTestRule<HomeActivityIntentTestRule, HomeActivity>,
        testBlock: () -> Unit,
    ) {
        val launcherIntent = Intent(Intent.ACTION_MAIN).apply {
            addCategory(Intent.CATEGORY_LAUNCHER)
        }

        activityTestRule.activityRule.withIntent(launcherIntent).launchActivity(launcherIntent)

        try {
            testBlock()
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }
}
