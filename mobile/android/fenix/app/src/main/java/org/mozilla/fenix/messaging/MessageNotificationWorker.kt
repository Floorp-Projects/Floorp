/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.messaging

import android.app.Notification
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.lifecycle.LifecycleService
import androidx.lifecycle.lifecycleScope
import androidx.work.CoroutineWorker
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import androidx.work.WorkerParameters
import kotlinx.coroutines.launch
import mozilla.components.service.nimbus.messaging.FxNimbusMessaging
import mozilla.components.service.nimbus.messaging.Message
import mozilla.components.support.base.ids.SharedIdsHelper
import mozilla.components.support.utils.BootUtils
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.onboarding.ensureMarketingChannelExists
import org.mozilla.fenix.utils.IntentUtils
import org.mozilla.fenix.utils.createBaseNotification
import java.util.concurrent.TimeUnit

const val CLICKED_MESSAGE_ID = "clickedMessageId"
const val DISMISSED_MESSAGE_ID = "dismissedMessageId"

/**
 * Background [CoroutineWorker] that polls Nimbus for available [Message]s at a given interval.
 * A [Notification] will be created using the configuration of the next highest priority [Message]
 * if it has not already been displayed.
 */
class MessageNotificationWorker(
    context: Context,
    workerParameters: WorkerParameters,
) : CoroutineWorker(context, workerParameters) {

    @SuppressWarnings("ReturnCount")
    override suspend fun doWork(): Result {
        val context = applicationContext

        val messaging = context.components.nimbus.messaging

        val nextMessage =
            messaging.getNextMessage(FenixMessageSurfaceId.NOTIFICATION)
                ?: return Result.success()

        val currentBootUniqueIdentifier = BootUtils.getBootIdentifier(context)
        //  Device has NOT been power cycled.
        if (nextMessage.hasShownThisCycle(currentBootUniqueIdentifier)) {
            return Result.success()
        }

        // Update message as displayed.
        messaging.onMessageDisplayed(nextMessage, currentBootUniqueIdentifier)

        context.components.notificationsDelegate.notify(
            MESSAGE_TAG,
            SharedIdsHelper.getIdForTag(context, nextMessage.id),
            buildNotification(
                context,
                nextMessage,
            ),
        )

        return Result.success()
    }

    private fun buildNotification(
        context: Context,
        message: Message,
    ): Notification {
        val onClickPendingIntent = createOnClickPendingIntent(context, message)
        val onDismissPendingIntent = createOnDismissPendingIntent(context, message)

        return createBaseNotification(
            context,
            ensureMarketingChannelExists(context),
            message.title,
            message.text,
            onClickPendingIntent,
            onDismissPendingIntent,
        )
    }

    private fun createOnClickPendingIntent(
        context: Context,
        message: Message,
    ): PendingIntent {
        val intent = Intent(context, NotificationClickedReceiverActivity::class.java)
        intent.putExtra(CLICKED_MESSAGE_ID, message.id)
        intent.addFlags(Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS)

        // Activity intent.
        return PendingIntent.getActivity(
            context,
            SharedIdsHelper.getNextIdForTag(context, NOTIFICATION_PENDING_INTENT_TAG),
            intent,
            IntentUtils.defaultIntentPendingFlags,
        )
    }

    private fun createOnDismissPendingIntent(
        context: Context,
        message: Message,
    ): PendingIntent {
        val intent = Intent(context, NotificationDismissedService::class.java)
        intent.putExtra(DISMISSED_MESSAGE_ID, message.id)

        // Service intent.
        return PendingIntent.getService(
            context,
            SharedIdsHelper.getNextIdForTag(context, NOTIFICATION_PENDING_INTENT_TAG),
            intent,
            IntentUtils.defaultIntentPendingFlags,
        )
    }

    companion object {
        private const val NOTIFICATION_PENDING_INTENT_TAG = "org.mozilla.fenix.message"
        private const val MESSAGE_TAG = "org.mozilla.fenix.message.tag"
        private const val MESSAGE_WORK_NAME = "org.mozilla.fenix.message.work"

        /**
         * Initialize the [CoroutineWorker] to begin polling Nimbus.
         */
        fun setMessageNotificationWorker(context: Context) {
            val messaging = FxNimbusMessaging.features.messaging
            val featureConfig = messaging.value()
            val notificationConfig = featureConfig.notificationConfig
            val pollingInterval = notificationConfig.refreshInterval.toLong()

            val messageWorkRequest = PeriodicWorkRequestBuilder<MessageNotificationWorker>(
                pollingInterval,
                TimeUnit.MINUTES,
            ).build()

            val instanceWorkManager = WorkManager.getInstance(context)
            instanceWorkManager.enqueueUniquePeriodicWork(
                MESSAGE_WORK_NAME,
                // We want to keep any existing scheduled work, unless
                // when we're under test.
                if (messaging.isUnderTest()) {
                    ExistingPeriodicWorkPolicy.CANCEL_AND_REENQUEUE
                } else {
                    ExistingPeriodicWorkPolicy.KEEP
                },
                messageWorkRequest,
            )
        }
    }
}

/**
 * When a [Message] [Notification] is dismissed by the user record telemetry data and update the
 * [Message.metadata].
 *
 * This Service is only intended to be used by the [MessageNotificationWorker.createOnDismissPendingIntent] function.
 */
class NotificationDismissedService : LifecycleService() {

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        super.onStartCommand(intent, flags, startId)

        if (intent != null) {
            val messaging = applicationContext.components.nimbus.messaging

            lifecycleScope.launch {
                // Get the relevant message.
                val message = intent.getStringExtra(DISMISSED_MESSAGE_ID)?.let { messageId ->
                    messaging.getMessage(messageId)
                }

                if (message != null) {
                    // Update message as 'dismissed'.
                    messaging.onMessageDismissed(message)
                }
            }
        }

        return START_REDELIVER_INTENT
    }
}

/**
 * When a [Message] [Notification] is clicked by the user record telemetry data and update the
 * [Message.metadata].
 *
 * This Activity is only intended to be used by the [MessageNotificationWorker.createOnClickPendingIntent] function.
 */
class NotificationClickedReceiverActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val messaging = applicationContext.components.nimbus.messaging

        lifecycleScope.launch {
            // Get the relevant message.
            val message = intent.getStringExtra(CLICKED_MESSAGE_ID)?.let { messageId ->
                messaging.getMessage(messageId)
            }

            if (message != null) {
                // Update message as 'clicked'.
                messaging.onMessageClicked(message)

                // Create the intent.
                val intent = messaging.getIntentForMessage(message)
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK)

                // Start the message intent.
                startActivity(intent)
            }
        }

        // End this activity.
        finish()
    }
}
