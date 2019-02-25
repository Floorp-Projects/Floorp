/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.concept.push

/**
 * A push message holds the information needed to pass the message on to the appropriate receiver.
 */
data class PushMessage(val type: MessageType, val data: String)

/**
 * The different kind of message types that a [PushMessage] can be.
 */
enum class MessageType {
    Services,
    ThirdParty,
    Unknown
}

