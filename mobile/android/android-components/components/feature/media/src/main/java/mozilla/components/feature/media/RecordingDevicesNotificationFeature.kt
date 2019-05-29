/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media

import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.utils.AllSessionsObserver
import mozilla.components.concept.engine.media.RecordingDevice
import mozilla.components.feature.media.notification.MediaNotificationChannel
import mozilla.components.support.base.ids.cancel
import mozilla.components.support.base.ids.notify

private const val NOTIFICATION_TAG = "mozac.feature.media.recordingDevices"

/**
 * Feature for displaying an ongoing notification while recording devices (camera, microphone) are used.
 *
 * This feature should be initialized globally (application scope) and only once.
 */
class RecordingDevicesNotificationFeature(
    private val context: Context,
    private val sessionManager: SessionManager
) {
    private var isShowingNotification: Boolean = false

    /**
     * Enable the media notification feature. An ongoing notification will be shown while recording devices (camera,
     * microphone) are used.
     */
    fun enable() {
        AllSessionsObserver(sessionManager, DevicesObserver(this))
            .start()
    }

    @Synchronized
    internal fun updateRecordingState(state: RecordingState) {
        if (state.isRecording && !isShowingNotification) {
            showNotification(state)
            isShowingNotification = true
        } else if (!state.isRecording && isShowingNotification) {
            hideNotification()
            isShowingNotification = false
        }
    }

    private fun showNotification(state: RecordingState) {
        val channelId = MediaNotificationChannel.ensureChannelExists(context)

        val intent = context.packageManager.getLaunchIntentForPackage(context.packageName)?.apply {
            flags = Intent.FLAG_ACTIVITY_NEW_TASK
        } ?: throw IllegalStateException("Package has no launcher intent")

        val pendingIntent = PendingIntent.getActivity(
            context, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT)

        val notification = NotificationCompat.Builder(context, channelId)
            .setSmallIcon(state.iconResource)
            .setContentTitle(context.getString(state.titleResource))
            .setPriority(NotificationCompat.PRIORITY_MAX)
            .setCategory(NotificationCompat.CATEGORY_CALL)
            .setContentIntent(pendingIntent)
            .setOngoing(true)
            .build()

        NotificationManagerCompat.from(context)
            .notify(context, NOTIFICATION_TAG, notification)
    }

    private fun hideNotification() {
        NotificationManagerCompat.from(context)
            .cancel(context, NOTIFICATION_TAG)
    }
}

private class DevicesObserver(
    private val feature: RecordingDevicesNotificationFeature
) : AllSessionsObserver.Observer {
    private val deviceMap = mutableMapOf<Session, List<RecordingDevice>>()

    override fun onRecordingDevicesChanged(session: Session, devices: List<RecordingDevice>) {
        if (devices.isEmpty()) {
            deviceMap.remove(session)
        } else {
            deviceMap[session] = devices
        }

        notifyFeature()
    }

    override fun onUnregisteredFromSession(session: Session) {
        deviceMap.remove(session)

        notifyFeature()
    }

    private fun notifyFeature() {
        val recordingDevices = deviceMap.values.flatten().filter { device ->
            device.status == RecordingDevice.Status.RECORDING
        }.distinctBy { device ->
            device.type
        }

        val isUsingCamera = recordingDevices.find { it.type == RecordingDevice.Type.CAMERA } != null
        val isUsingMicrophone = recordingDevices.find { it.type == RecordingDevice.Type.MICROPHONE } != null

        val state = when {
            isUsingCamera && isUsingMicrophone -> RecordingState.CameraAndMicrophone
            isUsingCamera -> RecordingState.Camera
            isUsingMicrophone -> RecordingState.Microphone
            else -> RecordingState.None
        }

        feature.updateRecordingState(state)
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
