/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.gleanplumb

import android.content.Context
import android.util.AtomicFile
import androidx.annotation.VisibleForTesting
import mozilla.components.support.ktx.util.readAndDeserialize
import mozilla.components.support.ktx.util.writeString
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

internal const val FILE_NAME = "nimbus_messages_metadata.json"

/**
 * A storage that persists [Message.Metadata] into disk.
 */
class OnDiskMessageMetadataStorage(
    private val context: Context,
) : MessageMetadataStorage {
    private val diskCacheLock = Any()

    @VisibleForTesting
    internal var metadataMap: MutableMap<String, Message.Metadata> = hashMapOf()

    override suspend fun getMetadata(): Map<String, Message.Metadata> {
        if (metadataMap.isEmpty()) {
            metadataMap = readFromDisk().toMutableMap()
        }
        return metadataMap
    }

    override suspend fun addMetadata(metadata: Message.Metadata): Message.Metadata {
        metadataMap[metadata.id] = metadata
        writeToDisk()
        return metadata
    }

    override suspend fun updateMetadata(metadata: Message.Metadata) {
        addMetadata(metadata)
    }

    @VisibleForTesting
    internal fun readFromDisk(): Map<String, Message.Metadata> {
        synchronized(diskCacheLock) {
            return getFile().readAndDeserialize {
                JSONArray(it).toMetadataMap()
            } ?: emptyMap()
        }
    }

    @VisibleForTesting
    internal fun writeToDisk() {
        synchronized(diskCacheLock) {
            val json = metadataMap.values.toList().fold("") { acc, next ->
                if (acc.isEmpty()) {
                    next.toJson()
                } else {
                    "$acc,${next.toJson()}"
                }
            }
            getFile().writeString { "[$json]" }
        }
    }

    private fun getFile(): AtomicFile {
        return AtomicFile(File(context.filesDir, FILE_NAME))
    }
}

internal fun JSONArray.toMetadataMap(): Map<String, Message.Metadata> {
    return (0 until length()).map { index ->
        getJSONObject(index).toMetadata()
    }.associateBy {
        it.id
    }
}

@Suppress("MaxLineLength") // To avoid adding any extra space to the string.
internal fun Message.Metadata.toJson(): String {
    return """{"id":"$id","displayCount":$displayCount,"pressed":$pressed,"dismissed":$dismissed,"lastTimeShown":$lastTimeShown,"latestBootIdentifier":"$latestBootIdentifier"}"""
}

internal fun JSONObject.toMetadata(): Message.Metadata {
    return Message.Metadata(
        id = optString("id"),
        displayCount = optInt("displayCount"),
        pressed = optBoolean("pressed"),
        dismissed = optBoolean("dismissed"),
        lastTimeShown = optLong("lastTimeShown"),
        latestBootIdentifier = optString("latestBootIdentifier"),
    )
}
