/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package mozilla.components.support.ktx.android.content

import android.app.ActivityManager
import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.content.Intent.ACTION_DIAL
import android.content.Intent.ACTION_SEND
import android.content.Intent.ACTION_SENDTO
import android.content.Intent.EXTRA_EMAIL
import android.content.Intent.EXTRA_SUBJECT
import android.content.Intent.EXTRA_TEXT
import android.content.Intent.FLAG_ACTIVITY_NEW_TASK
import android.content.pm.PackageManager.PERMISSION_GRANTED
import android.hardware.camera2.CameraManager
import android.net.Uri
import android.os.Process
import android.view.accessibility.AccessibilityManager
import androidx.annotation.AttrRes
import androidx.annotation.ColorInt
import androidx.annotation.DrawableRes
import androidx.annotation.VisibleForTesting
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.content.ContextCompat
import androidx.core.content.ContextCompat.checkSelfPermission
import androidx.core.content.getSystemService
import mozilla.components.support.base.log.Log
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.R
import mozilla.components.support.ktx.android.content.res.resolveAttribute

/**
 * The (visible) version name of the application, as specified by the <manifest> tag's versionName
 * attribute. E.g. "2.0".
 */
val Context.appVersionName: String?
    get() = packageManager.getPackageInfo(packageName, 0).versionName

/**
 * Returns the name (label) of the application or the package name as a fallback.
 */
val Context.appName: String
    get() = packageManager.getApplicationLabel(applicationInfo).toString()

/**
 * Returns whether or not the operating system is under low memory conditions.
 */
fun Context.isOSOnLowMemory(): Boolean {
    val activityManager: ActivityManager = getSystemService()!!
    return ActivityManager.MemoryInfo().also { memoryInfo ->
        activityManager.getMemoryInfo(memoryInfo)
    }.lowMemory
}

/**
 * Returns if a list of permission have been granted, if all the permission have been granted
 * returns true otherwise false.
 */
fun Context.isPermissionGranted(permission: Iterable<String>): Boolean {
    return permission.all { checkSelfPermission(this, it) == PERMISSION_GRANTED }
}
fun Context.isPermissionGranted(vararg permission: String): Boolean {
    return isPermissionGranted(permission.asIterable())
}

/**
 * Checks whether or not the device has a camera.
 *
 * @return true if a camera was found, otherwise false.
 */
@Suppress("TooGenericExceptionCaught")
fun Context.hasCamera(): Boolean {
    return try {
        val cameraManager: CameraManager? = getSystemService()
        cameraManager?.cameraIdList?.isNotEmpty() ?: false
    } catch (e: Exception) {
        false
    }
}

/**
 * Shares content via [ACTION_SEND] intent.
 *
 * @param text the data to be shared [EXTRA_TEXT]
 * @param subject of the intent [EXTRA_TEXT]
 * @return true it is able to share false otherwise.
 */
fun Context.share(text: String, subject: String = getString(R.string.mozac_support_ktx_share_dialog_title)): Boolean {
    return try {
        val intent = Intent(ACTION_SEND).apply {
            type = "text/plain"
            putExtra(EXTRA_SUBJECT, subject)
            putExtra(EXTRA_TEXT, text)
            flags = FLAG_ACTIVITY_NEW_TASK
        }

        val shareIntent = Intent.createChooser(intent, getString(R.string.mozac_support_ktx_menu_share_with)).apply {
            flags = FLAG_ACTIVITY_NEW_TASK
        }

        startActivity(shareIntent)
        true
    } catch (e: ActivityNotFoundException) {
        Log.log(Log.Priority.WARN, message = "No activity to share to found", throwable = e, tag = "Reference-Browser")
        false
    }
}

/**
 * Emails content via [ACTION_SENDTO] intent.
 *
 * @param address the email address to send to [EXTRA_EMAIL]
 * @param subject of the intent [EXTRA_TEXT]
 * @return true it is able to share email false otherwise.
 */
@SuppressWarnings("TooManyFunctions")
fun Context.email(
    address: String,
    subject: String = getString(R.string.mozac_support_ktx_share_dialog_title)
): Boolean {
    return try {
        val intent = Intent(ACTION_SENDTO, Uri.parse("mailto:$address"))
        intent.putExtra(EXTRA_SUBJECT, subject)

        val emailIntent = Intent.createChooser(
            intent,
            getString(R.string.mozac_support_ktx_menu_email_with)
        ).apply {
            flags = FLAG_ACTIVITY_NEW_TASK
        }

        startActivity(emailIntent)
        true
    } catch (e: ActivityNotFoundException) {
        Logger.warn("No activity found to handle email intent", throwable = e)
        false
    }
}

/**
 * Calls phone number via [ACTION_DIAL] intent.
 *
 * Note: we purposely use ACTION_DIAL rather than ACTION_CALL as the latter requires user permission
 * @param phoneNumber the phone number to send to [ACTION_DIAL]
 * @param subject of the intent [EXTRA_TEXT]
 * @return true it is able to share phone call false otherwise.
 */
fun Context.call(
    phoneNumber: String,
    subject: String = getString(R.string.mozac_support_ktx_share_dialog_title)
): Boolean {
    return try {
        val intent = Intent(ACTION_DIAL, Uri.parse("tel:$phoneNumber"))
        intent.putExtra(EXTRA_SUBJECT, subject)

        val callIntent = Intent.createChooser(
            intent,
            getString(R.string.mozac_support_ktx_menu_call_with)
        ).apply {
            flags = FLAG_ACTIVITY_NEW_TASK
        }

        startActivity(callIntent)
        true
    } catch (e: ActivityNotFoundException) {
        Logger.warn("No activity found to handle dial intent", throwable = e)
        false
    }
}

/**
 * Check if TalkBack service is enabled.
 *
 * (via https://stackoverflow.com/a/12362545/512580)
 */
inline val Context.isScreenReaderEnabled: Boolean
    get() = getSystemService<AccessibilityManager>()!!.isTouchExplorationEnabled

@VisibleForTesting
internal var isMainProcess: Boolean? = null

/**
 *  Returns true if we are running in the main process false otherwise.
 */
fun Context.isMainProcess(): Boolean {
    if (isMainProcess != null) return isMainProcess as Boolean

    val pid = Process.myPid()
    val activityManager: ActivityManager? = getSystemService()

    isMainProcess = activityManager?.runningAppProcesses.orEmpty().any { processInfo ->
        processInfo.pid == pid && processInfo.processName == packageName
    }

    return isMainProcess as Boolean
}

/**
 * Takes a function runs it only it if we are running in the main process, otherwise the function will not be executed.
 * @param [block] function to be executed in the main process.
 */
inline fun Context.runOnlyInMainProcess(block: () -> Unit) {
    if (isMainProcess()) {
        block()
    }
}

/**
 * Returns the color int corresponding to the attribute.
 */
@ColorInt
fun Context.getColorFromAttr(@AttrRes attr: Int) =
    ContextCompat.getColor(this, theme.resolveAttribute(attr))

/**
 * Returns a tinted drawable for the given resource ID.
 * @param resId ID of the drawable to load.
 * @param tint Tint color int to apply to the drawable.
 */
fun Context.getDrawableWithTint(@DrawableRes resId: Int, @ColorInt tint: Int) =
    AppCompatResources.getDrawable(this, resId)?.apply {
        mutate()
        setTint(tint)
    }
