/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.android

import android.Manifest.permission.POST_NOTIFICATIONS
import android.annotation.SuppressLint
import android.app.Notification
import android.os.Build
import android.os.RemoteException
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.annotation.RequiresApi
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.NotificationManagerCompat
import androidx.lifecycle.Lifecycle
import java.lang.ref.WeakReference
import java.util.WeakHashMap

typealias OnPermissionGranted = () -> Unit
typealias OnPermissionRejected = () -> Unit

/**
 * Exception thrown when a [NotificationsDelegate] is not bound to any [AppCompatActivity]
 */
class UnboundHandlerException(message: String) : Exception(message)

/**
 * Handles showing notifications and asking permission, if needed.
 *
 * @param notificationManagerCompat a reference to [NotificationManagerCompat].
 * @property onPermissionGranted optional callback for handling permission acceptance.
 * @property onPermissionRejected optional callback for handling permission refusal.
 */
class NotificationsDelegate(
    val notificationManagerCompat: NotificationManagerCompat,
) {
    private var onPermissionGranted: OnPermissionGranted = { }
    private var onPermissionRejected: OnPermissionRejected = { }
    private val notificationPermissionHandler:
        WeakHashMap<AppCompatActivity, WeakReference<ActivityResultLauncher<String>>> =
        WeakHashMap()

    /**
     * Provides the context for a permission request.
     */
    @Suppress("unused")
    fun bindToActivity(activity: AppCompatActivity) {
        val activityResultLauncher =
            activity.registerForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
                if (granted) {
                    onPermissionGranted.invoke()
                } else {
                    onPermissionRejected.invoke()
                }
            }

        notificationPermissionHandler[activity] = WeakReference(activityResultLauncher)
    }

    /**
     * Removes activity reference from the [NotificationsDelegate]
     */
    @Suppress("unused")
    fun unBindActivity(activity: AppCompatActivity) {
        notificationPermissionHandler.remove(activity)
    }

    /**
     * Checks if the post permission notification was previously granted.
     */
    private fun hasPostNotificationsPermission(): Boolean {
        return try {
            notificationManagerCompat.areNotificationsEnabled()
        } catch (e: RemoteException) {
            false
        }
    }

    /**
     * Handles showing notifications and asking permission, if needed.
     *
     * @param notificationTag the string identifier for a notification. Can be null
     * @param notificationId ID of the notification. The pair (tag, id) must be unique throughout the app
     * @param notification the notification to post to the system
     * @param onPermissionGranted optional callback for handling permission acceptance,
     * in addition to showing the notification.
     * Note that it will also be called when the permission is already granted.
     * @param onPermissionRejected optional callback for handling permission refusal.
     */
    @SuppressLint("MissingPermission", "NotifyUsage")
    fun notify(
        notificationTag: String? = null,
        notificationId: Int,
        notification: Notification,
        onPermissionGranted: OnPermissionGranted = { },
        onPermissionRejected: OnPermissionRejected = { },
        showPermissionRationale: Boolean = false,
    ) {
        if (hasPostNotificationsPermission()) {
            notificationManagerCompat.notify(notificationTag, notificationId, notification)
            onPermissionGranted.invoke()
        } else {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                requestNotificationPermission(
                    onPermissionGranted = {
                        notificationManagerCompat.notify(
                            notificationTag,
                            notificationId,
                            notification,
                        )
                        onPermissionGranted.invoke()
                    },
                    onPermissionRejected = onPermissionRejected,
                    showPermissionRationale = showPermissionRationale,
                )
            } else {
                // this means we cannot show standard notifications without user changing it from OS Settings
                // redirect to that, or maybe show in-app notifications? See https://bugzilla.mozilla.org/show_bug.cgi?id=1814863
                // for crash notifications we could show the prompt instead.
            }
        }
    }

    /**
     * Handles requesting the notification permission.
     *
     * @param onPermissionGranted optional callback for handling permission acceptance.
     * @param onPermissionRejected optional callback for handling permission refusal.
     */
    @RequiresApi(Build.VERSION_CODES.TIRAMISU)
    fun requestNotificationPermission(
        onPermissionGranted: OnPermissionGranted = { },
        onPermissionRejected: OnPermissionRejected = { },
        showPermissionRationale: Boolean = false,
    ) {
        // some clients might request notification permission when it is already granted,
        // so we should check first.
        if (hasPostNotificationsPermission()) {
            onPermissionGranted.invoke()
            return
        }

        this.onPermissionGranted = onPermissionGranted
        this.onPermissionRejected = onPermissionRejected

        if (notificationPermissionHandler.isEmpty()) {
            throw UnboundHandlerException("You must bind the NotificationPermissionHandler to an activity")
        }

        if (showPermissionRationale) {
            showPermissionRationale(onPermissionGranted, onPermissionRejected)
        } else {
            notificationPermissionHandler.entries.firstOrNull {
                it.key.lifecycle.currentState.isAtLeast(Lifecycle.State.STARTED)
            }?.value?.get()?.launch(POST_NOTIFICATIONS)
        }
    }

    /**
     * Handles displaying a notification pre permission prompt.
     *
     * @param onPermissionGranted optional callback for handling permission acceptance.
     * @param onPermissionRejected optional callback for handling permission refusal.
     */
    @Suppress("UNUSED_PARAMETER")
    private fun showPermissionRationale(
        onPermissionGranted: OnPermissionGranted,
        onPermissionRejected: OnPermissionRejected,
    ) {
        // Content to be decided. Could follow existing NotificationPermissionDialogScreen.
    }
}
