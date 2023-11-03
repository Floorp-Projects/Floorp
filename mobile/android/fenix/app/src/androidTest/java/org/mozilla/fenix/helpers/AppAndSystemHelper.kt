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
import android.os.storage.StorageManager
import android.os.storage.StorageVolume
import android.provider.Settings
import android.util.Log
import androidx.annotation.RequiresApi
import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.test.espresso.Espresso
import androidx.test.espresso.IdlingRegistry
import androidx.test.espresso.IdlingResource
import androidx.test.espresso.intent.Intents
import androidx.test.espresso.intent.Intents.intended
import androidx.test.espresso.intent.matcher.IntentMatchers
import androidx.test.espresso.intent.matcher.IntentMatchers.toPackage
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.ActivityTestRule
import androidx.test.runner.permission.PermissionRequester
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiObject
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import junit.framework.AssertionFailedError
import org.junit.Assert
import org.junit.Assert.assertEquals
import org.mozilla.fenix.Config
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.customtabs.ExternalAppBrowserActivity
import org.mozilla.fenix.helpers.Constants.PackageName.YOUTUBE_APP
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

    @RequiresApi(Build.VERSION_CODES.R)
    fun deleteDownloadedFileOnStorage(fileName: String) {
        val storageManager: StorageManager? = TestHelper.appContext.getSystemService(Context.STORAGE_SERVICE) as StorageManager?
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
    }

    @RequiresApi(Build.VERSION_CODES.R)
    fun clearDownloadsFolder() {
        val storageManager: StorageManager? = TestHelper.appContext.getSystemService(Context.STORAGE_SERVICE) as StorageManager?
        val storageVolumes = storageManager!!.storageVolumes
        val storageVolume: StorageVolume = storageVolumes[0]
        val downloadsFolder = File(storageVolume.directory!!.path + "/Download/")

        // Check if the downloads folder exists
        if (downloadsFolder.exists() && downloadsFolder.isDirectory) {
            val files = downloadsFolder.listFiles()

            // Check if the folder is not empty
            if (files != null && files.isNotEmpty()) {
                // Delete all files in the folder
                for (file in files) {
                    file.delete()
                }
            }
        }
    }

    fun setNetworkEnabled(enabled: Boolean) {
        val networkDisconnectedIdlingResource = NetworkConnectionIdlingResource(false)
        val networkConnectedIdlingResource = NetworkConnectionIdlingResource(true)

        when (enabled) {
            true -> {
                TestHelper.mDevice.executeShellCommand("svc data enable")
                TestHelper.mDevice.executeShellCommand("svc wifi enable")

                // Wait for network connection to be completely enabled
                IdlingRegistry.getInstance().register(networkConnectedIdlingResource)
                Espresso.onIdle {
                    IdlingRegistry.getInstance().unregister(networkConnectedIdlingResource)
                }
            }

            false -> {
                TestHelper.mDevice.executeShellCommand("svc data disable")
                TestHelper.mDevice.executeShellCommand("svc wifi disable")

                // Wait for network connection to be completely disabled
                IdlingRegistry.getInstance().register(networkDisconnectedIdlingResource)
                Espresso.onIdle {
                    IdlingRegistry.getInstance().unregister(networkDisconnectedIdlingResource)
                }
            }
        }
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
                Intents.intended(IntentMatchers.toPackage(appPackageName))
            } catch (e: AssertionFailedError) {
                e.printStackTrace()
            }
        } else {
            TestHelper.mDevice.waitNotNull(
                Until.findObject(By.text("Could not open file")),
                TestAssetHelper.waitingTime,
            )
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
        val activityManager = TestHelper.appContext.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager

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

    fun putAppToBackground() {
        mDevice.pressRecentApps()
        mDevice.findObject(UiSelector().resourceId("${TestHelper.packageName}:id/container")).waitUntilGone(
            TestAssetHelper.waitingTime,
        )
    }

    fun bringAppToForeground() {
        mDevice.pressRecentApps()
        mDevice.findObject(UiSelector().resourceId("${TestHelper.packageName}:id/container")).waitForExists(
            TestAssetHelper.waitingTime,
        )
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
