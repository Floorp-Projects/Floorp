/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.messaging

import mozilla.components.service.nimbus.messaging.Message

/**
 * Controls all the interactions with a [Message].
 */
interface MessageController {
    /**
     * Indicates the provided [message] was pressed by a user.
     */
    fun onMessagePressed(message: Message)

    /**
     * Indicates the provided [message] was dismissed by a user.
     */
    fun onMessageDismissed(message: Message)
}
