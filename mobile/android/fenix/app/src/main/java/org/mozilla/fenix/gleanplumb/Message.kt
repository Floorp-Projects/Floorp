/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.gleanplumb

import org.mozilla.fenix.nimbus.MessageData
import org.mozilla.fenix.nimbus.MessageSurfaceId
import org.mozilla.fenix.nimbus.StyleData

/**
 * A data class that holds a representation of GleanPlum message from Nimbus.
 *
 * @param id identifies a message as unique.
 * @param data Data information provided from Nimbus.
 * @param action A strings that represents which action should be performed
 * after a message is clicked.
 * @param style Indicates how a message should be styled.
 * @param triggers A list of strings corresponding to targeting expressions. The message
 * will be shown if all expressions `true`.
 * @param metadata Metadata that help to identify if a message should shown.
 */
data class Message(
    val id: String,
    val data: MessageData,
    val action: String,
    val style: StyleData,
    val triggers: List<String>,
    val metadata: Metadata,
) {
    val maxDisplayCount: Int
        get() = style.maxDisplayCount

    val priority: Int
        get() = style.priority

    val surface: MessageSurfaceId
        get() = data.surface

    val isExpired: Boolean
        get() = metadata.displayCount >= maxDisplayCount

    /**
     * A data class that holds metadata that help to identify if a message should shown.
     *
     * @param id identifies a message as unique.
     * @param displayCount Indicates how many times a message is displayed.
     * @param pressed Indicates if a message has been clicked.
     * @param dismissed Indicates if a message has been closed.
     * @param lastTimeShown A timestamp indicating when was the last time, the message was shown.
     * @param latestBootIdentifier A unique boot identifier for when the message was last displayed
     * (this may be a boot count or a boot id).
     */
    data class Metadata(
        val id: String,
        val displayCount: Int = 0,
        val pressed: Boolean = false,
        val dismissed: Boolean = false,
        val lastTimeShown: Long = 0L,
        val latestBootIdentifier: String? = null,
    )
}
