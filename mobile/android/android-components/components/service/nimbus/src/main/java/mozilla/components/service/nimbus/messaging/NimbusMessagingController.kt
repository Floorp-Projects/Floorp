/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.nimbus.messaging

import android.content.Intent
import android.net.Uri
import androidx.annotation.VisibleForTesting
import androidx.core.net.toUri
import mozilla.components.service.nimbus.GleanMetrics.Messaging as GleanMessaging

/**
 * Bookkeeping for message actions in terms of Glean messages and the messaging store.
 *
 * @param messagingStorage a NimbusMessagingStorage instance
 * @param deepLinkScheme the deepLinkScheme for the app
 * @param now will be used to get the current time
 */
open class NimbusMessagingController(
    private val messagingStorage: NimbusMessagingStorage,
    private val deepLinkScheme: String,
) : NimbusMessagingControllerInterface {
    /**
     * Records telemetry and metadata for a newly processed displayed message.
     */
    override suspend fun onMessageDisplayed(displayedMessage: Message, bootIdentifier: String?): Message {
        sendShownMessageTelemetry(displayedMessage.id)
        val nextMessage = messagingStorage.onMessageDisplayed(displayedMessage, bootIdentifier)
        if (nextMessage.isExpired) {
            sendExpiredMessageTelemetry(nextMessage.id)
        }
        return nextMessage
    }

    /**
     * Called when a message has been dismissed by the user.
     *
     * Records a messageDismissed event, and records that the message
     * has been dismissed.
     */
    override suspend fun onMessageDismissed(message: Message) {
        val messageMetadata = message.metadata
        sendDismissedMessageTelemetry(messageMetadata.id)
        val updatedMetadata = messageMetadata.copy(dismissed = true)
        messagingStorage.updateMetadata(updatedMetadata)
    }

    /**
     * Called once the user has clicked on a message.
     *
     * This records that the message has been clicked on, but does not record a
     * glean event. That should be done via [processMessageActionToUri].
     */
    override suspend fun onMessageClicked(message: Message) {
        val messageMetadata = message.metadata
        val updatedMetadata = messageMetadata.copy(pressed = true)
        messagingStorage.updateMetadata(updatedMetadata)
    }

    /**
     * Create and return the relevant [Intent] for the given [Message].
     *
     * @param message the [Message] to create the [Intent] for.
     * @return an [Intent] using the processed [Message].
     */
    override fun getIntentForMessage(message: Message) = Intent(
        Intent.ACTION_VIEW,
        processMessageActionToUri(message),
    )

    /**
     * Will attempt to get the [Message] for the given [id].
     *
     * @param id the [Message.id] of the [Message] to try to match.
     * @return the [Message] with a matching [id], or null if no [Message] has a matching [id].
     */
    override suspend fun getMessage(id: String): Message? {
        return messagingStorage.getMessage(id)
    }

    /**
     * The [message] action needs to be examined for string substitutions
     * and any `uuid` needs to be recorded in the Glean event.
     *
     * We call this `process` as it has a side effect of logging a Glean event while it
     * creates a URI string for the message action.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun processMessageActionToUri(message: Message): Uri {
        val (uuid, action) = messagingStorage.generateUuidAndFormatMessage(message)
        sendClickedMessageTelemetry(message.id, uuid)

        return convertActionIntoDeepLinkSchemeUri(action)
    }

    private fun sendDismissedMessageTelemetry(messageId: String) {
        GleanMessaging.messageDismissed.record(GleanMessaging.MessageDismissedExtra(messageId))
    }

    private fun sendShownMessageTelemetry(messageId: String) {
        GleanMessaging.messageShown.record(GleanMessaging.MessageShownExtra(messageId))
    }

    private fun sendExpiredMessageTelemetry(messageId: String) {
        GleanMessaging.messageExpired.record(GleanMessaging.MessageExpiredExtra(messageId))
    }

    private fun sendClickedMessageTelemetry(messageId: String, uuid: String?) {
        GleanMessaging.messageClicked.record(
            GleanMessaging.MessageClickedExtra(messageKey = messageId, actionUuid = uuid),
        )
    }

    private fun convertActionIntoDeepLinkSchemeUri(action: String): Uri =
        if (action.startsWith("://")) {
            "$deepLinkScheme$action".toUri()
        } else {
            action.toUri()
        }

    override suspend fun getMessages(): List<Message> =
        messagingStorage.getMessages()

    override suspend fun getNextMessage(surfaceId: MessageSurfaceId) =
        getNextMessage(surfaceId, getMessages())

    override fun getNextMessage(surfaceId: MessageSurfaceId, messages: List<Message>) =
        messagingStorage.getNextMessage(surfaceId, messages)
}
