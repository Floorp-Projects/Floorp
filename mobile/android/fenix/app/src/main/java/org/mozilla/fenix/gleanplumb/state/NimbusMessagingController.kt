/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.gleanplumb.state

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import org.mozilla.fenix.GleanMetrics.Messaging
import org.mozilla.fenix.gleanplumb.Message
import org.mozilla.fenix.gleanplumb.NimbusMessagingStorage

/**
 * Class extracted from [MessagingMiddleware] to do the bookkeeping for message actions, in terms
 * of Glean messages and the messaging store.
 */
open class NimbusMessagingController(
    protected val messagingStorage: NimbusMessagingStorage,
    protected val coroutineScope: CoroutineScope = CoroutineScope(Dispatchers.IO),
    private val clock: () -> Long = { System.currentTimeMillis() },
) {
    fun onMessageDisplayed(oldMessage: Message): Message {
        sendShownMessageTelemetry(oldMessage.id)
        val newMetadata = oldMessage.metadata.copy(
            displayCount = oldMessage.metadata.displayCount + 1,
            lastTimeShown = clock(),
        )
        val newMessage = oldMessage.copy(
            metadata = newMetadata,
        )

        coroutineScope.launch {
            messagingStorage.updateMetadata(newMetadata)
        }

        if (newMessage.isExpired) {
            sendExpiredMessageTelemetry(newMessage.id)
        }

        return newMessage
    }

    fun onMessageDismissed(message: Message) {
        coroutineScope.launch {
            val updatedMetadata = message.metadata.copy(dismissed = true)
            messagingStorage.updateMetadata(updatedMetadata)
        }
    }

    fun onMessageClicked(message: Message) {
        coroutineScope.launch {
            val updatedMetadata = message.metadata.copy(pressed = true)
            messagingStorage.updateMetadata(updatedMetadata)
        }
    }

    private fun sendShownMessageTelemetry(messageId: String) {
        Messaging.messageShown.record(Messaging.MessageShownExtra(messageId))
    }

    private fun sendExpiredMessageTelemetry(messageId: String) {
        Messaging.messageExpired.record(Messaging.MessageExpiredExtra(messageId))
    }
}
