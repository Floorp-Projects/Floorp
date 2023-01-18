/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.gleanplumb

import android.net.Uri
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.GleanMetrics.Messaging

/**
 * Class extracted from [MessagingMiddleware] to do the bookkeeping for message actions, in terms
 * of Glean messages and the messaging store.
 */
class NimbusMessagingController(
    private val messagingStorage: NimbusMessagingStorage,
    private val now: () -> Long = { System.currentTimeMillis() },
) {
    /**
     * Called when a message is just about to be shown to the user.
     *
     * This updates the display count, and expires the message if necessary.
     *
     * Records glean events for messageShown and messageExpired.
     */
    suspend fun processDisplayedMessage(oldMessage: Message): Message {
        sendShownMessageTelemetry(oldMessage.id)
        val newMetadata = oldMessage.metadata.copy(
            displayCount = oldMessage.metadata.displayCount + 1,
            lastTimeShown = now(),
        )
        val newMessage = oldMessage.copy(
            metadata = newMetadata,
        )

        messagingStorage.updateMetadata(newMetadata)

        if (newMessage.isExpired) {
            sendExpiredMessageTelemetry(newMessage.id)
        }

        return newMessage
    }

    /**
     * Called when a message has been dismissed by the user.
     *
     * Records a messageDismissed event, and records that the message
     * has been dismissed.
     */
    suspend fun onMessageDismissed(message: Message) {
        sendDismissedMessageTelemetry(message.id)
        val updatedMetadata = message.metadata.copy(dismissed = true)
        messagingStorage.updateMetadata(updatedMetadata)
    }

    /**
     * Once a message is clicked, the action needs to be examined for string substitutions
     * and any `uuid` needs to be recorded in the Glean event.
     *
     * We call this `process` as it has a side effect of logging a Glean event while it
     * creates a URI string for
     */
    fun processMessageAction(message: Message): String {
        val (uuid, action) = messagingStorage.getMessageAction(message.action)
        sendClickedMessageTelemetry(message.id, uuid)

        return if (action.startsWith("http", ignoreCase = true)) {
            "${BuildConfig.DEEP_LINK_SCHEME}://open?url=${Uri.encode(action)}"
        } else if (action.startsWith("://")) {
            "${BuildConfig.DEEP_LINK_SCHEME}$action"
        } else {
            action
        }
    }

    /**
     * Called once the user has clicked on a message.
     *
     * This records that the message has been clicked on, but does not record a
     * glean event. That should be done via [processMessageAction].
     */
    suspend fun onMessageClicked(message: Message) {
        val updatedMetadata = message.metadata.copy(pressed = true)
        messagingStorage.updateMetadata(updatedMetadata)
    }

    private fun sendDismissedMessageTelemetry(messageId: String) {
        Messaging.messageDismissed.record(Messaging.MessageDismissedExtra(messageId))
    }

    private fun sendShownMessageTelemetry(messageId: String) {
        Messaging.messageShown.record(Messaging.MessageShownExtra(messageId))
    }

    private fun sendExpiredMessageTelemetry(messageId: String) {
        Messaging.messageExpired.record(Messaging.MessageExpiredExtra(messageId))
    }

    private fun sendClickedMessageTelemetry(messageId: String, uuid: String?) {
        Messaging.messageClicked.record(
            Messaging.MessageClickedExtra(
                messageKey = messageId,
                actionUuid = uuid,
            ),
        )
    }
}
