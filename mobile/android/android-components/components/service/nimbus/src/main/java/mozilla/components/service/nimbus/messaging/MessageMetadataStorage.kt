/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.nimbus.messaging

/**
 * A storage that persists [Message.Metadata] into disk.
 */
interface MessageMetadataStorage {
    /**
     * Provide all the message metadata saved in the storage.
     */
    suspend fun getMetadata(): Map<String, Message.Metadata>

    /**
     * Given a [metadata] add the message metadata on the storage.
     * @return the added message on the [MessageMetadataStorage]
     */
    suspend fun addMetadata(metadata: Message.Metadata): Message.Metadata

    /**
     * Given a [metadata] update the message metadata on the storage.
     */
    suspend fun updateMetadata(metadata: Message.Metadata)
}
