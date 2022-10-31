/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.middleware

import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import androidx.annotation.VisibleForTesting
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.concept.engine.media.RecordingDevice
import mozilla.components.feature.media.R
import mozilla.components.feature.media.notification.MediaNotificationChannel
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.base.ids.SharedIdsHelper
import mozilla.components.support.utils.PendingIntentUtils

private const val NOTIFICATION_TAG = "mozac.feature.media.recordingDevices"
private const val NOTIFICATION_ID = 1
private const val PENDING_INTENT_TAG = "mozac.feature.media.pendingintent"

/**
 * Middleware for displaying an ongoing notification while recording devices (camera, microphone)
 * are used by web content.
 */
class RecordingDevicesMiddleware(
    private val context: Context,
) : Middleware<BrowserState, BrowserAction> {
    private var isShowingNotification: Boolean = false

    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction,
    ) {
        next(action)

        // Whenever the recording devices of a tab change or tabs get added/removed then process
        // the current list and show/hide the notification.
        if (
            action is ContentAction.SetRecordingDevices ||
            action is TabListAction ||
            action is CustomTabListAction
        ) {
            process(context.state)
        }
    }

    private fun process(state: BrowserState) {
        val devices = state.tabs
            .map { tab -> tab.content.recordingDevices }
            .flatten()
            .filter { device -> device.status == RecordingDevice.Status.RECORDING }
            .distinctBy { device -> device.type }

        val isUsingCamera = devices.find { it.type == RecordingDevice.Type.CAMERA } != null
        val isUsingMicrophone = devices.find { it.type == RecordingDevice.Type.MICROPHONE } != null

        val recordingState = when {
            isUsingCamera && isUsingMicrophone -> RecordingState.CameraAndMicrophone
            isUsingCamera -> RecordingState.Camera
            isUsingMicrophone -> RecordingState.Microphone
            else -> RecordingState.None
        }

        updateNotification(recordingState)
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun updateNotification(recordingState: RecordingState) {
        if (recordingState.isRecording && !isShowingNotification) {
            showNotification(recordingState)
            isShowingNotification = true
        } else if (!recordingState.isRecording && isShowingNotification) {
            hideNotification()
            isShowingNotification = false
        }
    }

    private fun showNotification(recordingState: RecordingState) {
        val channelId = MediaNotificationChannel.ensureChannelExists(context)

        val intent = context.packageManager.getLaunchIntentForPackage(context.packageName)?.apply {
            flags = Intent.FLAG_ACTIVITY_NEW_TASK
        } ?: throw IllegalStateException("Package has no launcher intent")

        val pendingIntent = PendingIntent.getActivity(
            context,
            SharedIdsHelper.getIdForTag(context, PENDING_INTENT_TAG),
            intent,
            PendingIntentUtils.defaultFlags or PendingIntent.FLAG_UPDATE_CURRENT,
        )

        val notification = NotificationCompat.Builder(context, channelId)
            .setSmallIcon(recordingState.iconResource)
            .setContentTitle(context.getString(recordingState.titleResource))
            .setPriority(NotificationCompat.PRIORITY_MAX)
            .setCategory(NotificationCompat.CATEGORY_CALL)
            .setContentIntent(pendingIntent)
            .setOngoing(true)
            .build()

        NotificationManagerCompat.from(context)
            .notify(NOTIFICATION_TAG, NOTIFICATION_ID, notification)
    }

    private fun hideNotification() {
        NotificationManagerCompat.from(context)
            .cancel(NOTIFICATION_TAG, NOTIFICATION_ID)
    }
}

internal sealed class RecordingState {
    abstract val iconResource: Int
    abstract val titleResource: Int

    val isRecording
        get() = this !is None

    object CameraAndMicrophone : RecordingState() {
        override val iconResource = R.drawable.mozac_ic_video
        override val titleResource = R.string.mozac_feature_media_sharing_camera_and_microphone
    }

    object Camera : RecordingState() {
        override val iconResource = R.drawable.mozac_ic_video
        override val titleResource = R.string.mozac_feature_media_sharing_camera
    }

    object Microphone : RecordingState() {
        override val iconResource = R.drawable.mozac_ic_microphone
        override val titleResource = R.string.mozac_feature_media_sharing_microphone
    }

    object None : RecordingState() {
        override val iconResource: Int
            get() = throw UnsupportedOperationException()

        override val titleResource: Int
            get() = throw UnsupportedOperationException()
    }
}
