/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.messaging

import mozilla.components.service.nimbus.messaging.Message
import mozilla.components.service.nimbus.messaging.MessageSurfaceId

/**
 * Represent all the state related to the Messaging framework.
 * @param messages Indicates all the available messages.
 * @param messageToShow Indicates the message that should be shown to users,
 * if it is null means there is not message that is eligible to be shown to users.
 */
data class MessagingState(
    val messages: List<Message> = emptyList(),
    val messageToShow: Map<MessageSurfaceId, Message> = emptyMap(),
)
