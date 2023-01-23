/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.gleanplumb

import android.app.Notification
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import androidx.core.app.NotificationManagerCompat
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.PeriodicWorkRequest
import androidx.work.WorkManager
import androidx.work.Worker
import androidx.work.WorkerParameters
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import mozilla.components.support.base.ids.SharedIdsHelper
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.nimbus.FxNimbus
import org.mozilla.fenix.nimbus.MessageSurfaceId
import org.mozilla.fenix.onboarding.MARKETING_CHANNEL_ID
import org.mozilla.fenix.utils.IntentUtils
import org.mozilla.fenix.utils.createBaseNotification
import java.util.concurrent.TimeUnit

/**
 * Background [Worker] that polls Nimbus for available [Message]s at a given interval.
 * A [Notification] will be created using the configuration of the next highest priority [Message]
 * if it has not already been displayed.
 */
class MessageNotificationWorker(
    context: Context,
    workerParameters: WorkerParameters,
) : Worker(context, workerParameters) {

    @OptIn(DelicateCoroutinesApi::class) // GlobalScope usage
    override fun doWork(): Result {
        GlobalScope.launch(Dispatchers.IO) {
            val context = applicationContext
            val messagingStorage = context.components.analytics.messagingStorage
            val messages = messagingStorage.getMessages()
            val nextMessage =
                messagingStorage.getNextMessage(MessageSurfaceId.NOTIFICATION, messages)

            if (nextMessage == null || !nextMessage.shouldDisplayMessage()) {
                return@launch
            }

            val nimbusMessagingController = NimbusMessagingController(messagingStorage)

            // Update message as displayed.
            val messageAsDisplayed =
                nimbusMessagingController.updateMessageAsDisplayed(nextMessage)
            nimbusMessagingController.onMessageDisplayed(messageAsDisplayed)

            // Generate the processed Message action
            val processedAction = nimbusMessagingController.processMessageActionToUri(nextMessage)
            val actionIntent = Intent(Intent.ACTION_VIEW, processedAction)

            NotificationManagerCompat.from(context).notify(
                MESSAGE_TAG,
                SharedIdsHelper.getNextIdForTag(context, nextMessage.id),
                buildNotification(nextMessage, actionIntent),
            )
        }

        return Result.success()
    }

    private fun Message.shouldDisplayMessage() = metadata.displayCount == 0

    private fun buildNotification(message: Message, intent: Intent): Notification {
        with(applicationContext) {
            val pendingIntent = PendingIntent.getActivity(
                this,
                SharedIdsHelper.getNextIdForTag(this, NOTIFICATION_PENDING_INTENT_TAG),
                intent,
                IntentUtils.defaultIntentPendingFlags,
            )

            return createBaseNotification(
                this,
                MARKETING_CHANNEL_ID,
                message.data.title,
                message.data.text,
                pendingIntent,
            )
        }
    }

    companion object {
        private const val NOTIFICATION_PENDING_INTENT_TAG = "org.mozilla.fenix.message"
        private const val MESSAGE_TAG = "org.mozilla.fenix.message.tag"
        private const val MESSAGE_WORK_NAME = "org.mozilla.fenix.message.work"

        /**
         * Initialize the [Worker] to begin polling Nimbus.
         */
        fun setMessageNotificationWorker(context: Context) {
            val featureConfig = FxNimbus.features.messaging.value()
            val notificationConfig = featureConfig.notificationConfig
            val pollingInterval = notificationConfig.pollingInterval.toLong()

            val messageWorkRequest = PeriodicWorkRequest.Builder(
                MessageNotificationWorker::class.java,
                pollingInterval,
                TimeUnit.MINUTES,
            ) // Only start polling after the given interval
                .setInitialDelay(pollingInterval, TimeUnit.MINUTES)
                .build()

            val instanceWorkManager = WorkManager.getInstance(context)
            instanceWorkManager.enqueueUniquePeriodicWork(
                MESSAGE_WORK_NAME,
                // We want to keep any existing scheduled work
                ExistingPeriodicWorkPolicy.KEEP,
                messageWorkRequest,
            )
        }
    }
}
