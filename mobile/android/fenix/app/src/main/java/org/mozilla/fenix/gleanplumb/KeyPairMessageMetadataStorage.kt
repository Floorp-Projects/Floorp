/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.gleanplumb

/* Dummy implementation until we provide full implementation.
* This will covered on https://github.com/mozilla-mobile/fenix/issues/24222
* */
class KeyPairMessageMetadataStorage : MessageMetadataStorage {
    override fun getMetadata(): List<Message.Metadata> {
        return listOf(
            Message.Metadata(
                id = "eu-tracking-protection-for-ireland",
                displayCount = 0,
                pressed = false,
                dismissed = false
            )
        )
    }

    override fun addMetadata(metadata: Message.Metadata): Message.Metadata {
        return metadata
    }

    @SuppressWarnings("EmptyFunctionBlock")
    override fun updateMetadata(metadata: Message.Metadata) {
    }
}
