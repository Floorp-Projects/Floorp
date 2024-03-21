/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.remotesettings

import android.util.AtomicFile
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.serialization.KSerializer
import kotlinx.serialization.Serializable
import kotlinx.serialization.SerializationException
import kotlinx.serialization.descriptors.PrimitiveKind
import kotlinx.serialization.descriptors.PrimitiveSerialDescriptor
import kotlinx.serialization.descriptors.SerialDescriptor
import kotlinx.serialization.encoding.Decoder
import kotlinx.serialization.encoding.Encoder
import kotlinx.serialization.json.Json
import mozilla.appservices.remotesettings.Attachment
import mozilla.appservices.remotesettings.RemoteSettings
import mozilla.appservices.remotesettings.RemoteSettingsConfig
import mozilla.appservices.remotesettings.RemoteSettingsException
import mozilla.appservices.remotesettings.RemoteSettingsRecord
import mozilla.appservices.remotesettings.RemoteSettingsResponse
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.util.writeString
import org.json.JSONObject
import java.io.File
import java.io.FileNotFoundException
import java.io.IOException
import java.net.URL

/**
 * Helper class to download collections from remote settings in app services.
 *
 * @property storageRootDirectory The top level app-local storage directory.
 * @property serverUrl The optional url for the settings server. If not specified, the standard server will be used.
 * @property bucketName The optional name of the bucket containing the collection on the server.
 * If not specified, the name of the bucket "main" [or the const if updated] will be used.
 * @property collectionName The name of the collection for the settings server.
 */

class RemoteSettingsClient(
    private val storageRootDirectory: File,
    private val serverUrl: String = "https://firefox.settings.services.mozilla.com",
    private val bucketName: String = "main",
    private val collectionName: String,
) {

    private val config = RemoteSettingsConfig(
        serverUrl = serverUrl,
        bucketName = bucketName,
        collectionName = collectionName,
    )
    private val serverHostName = URL(serverUrl).host
    private val path = "${storageRootDirectory.path}/$serverHostName/$bucketName/$collectionName"

    @VisibleForTesting
    internal var file = File(path)

    /**
     * Fetches a response that includes Remote Settings records and the last time the collection was modified.
     */
    suspend fun fetch(): RemoteSettingsResult = withContext(Dispatchers.IO) {
        try {
            val serverRecords = RemoteSettings(config).use {
                it.getRecords()
            }
            RemoteSettingsResult.Success(serverRecords)
        } catch (e: RemoteSettingsException) {
            Logger.error(e.message.toString())
            RemoteSettingsResult.NetworkFailure(e)
        } catch (e : NullPointerException) {
            Logger.error(e.message.toString())
            RemoteSettingsResult.NetworkFailure(e)
        }
    }

    /**
     * Updates the local storage with [response] data.
     * The full response is required because the `lastModified` property may be used for future comparisons.
     */
    suspend fun write(response: RemoteSettingsResponse): RemoteSettingsResult = withContext(Dispatchers.IO) {
        try {
            val jsonString = Json.encodeToString(
                SerializableRemoteSettingsResponse.serializer(),
                response.toSerializable(),
            )
            AtomicFile(file).writeString { jsonString }
            RemoteSettingsResult.Success(response)
        } catch (e: IOException) {
            RemoteSettingsResult.DiskFailure(e)
        } catch (e: SerializationException) {
            RemoteSettingsResult.DiskFailure(e)
        }
    }

    /**
     * Reads all response for a collection found in the local storage.
     */
    suspend fun read(): RemoteSettingsResult = withContext(Dispatchers.IO) {
        try {
            if (!file.exists()) {
                RemoteSettingsResult.DiskFailure(FileNotFoundException("File not found"))
            } else {
                val jsonString = file.readText()
                val response = Json.decodeFromString<SerializableRemoteSettingsResponse>(jsonString)
                RemoteSettingsResult.Success(response.toRemoteSettingsResponse())
            }
        } catch (e: IOException) {
            RemoteSettingsResult.DiskFailure(e)
        } catch (e: SerializationException) {
            RemoteSettingsResult.DiskFailure(e)
        }
    }
}

/**
 * Reads files from local storage. If file is empty, fetches the file from the remote server.
 */
suspend fun RemoteSettingsClient.readOrFetch(): RemoteSettingsResult {
    val readResult = read()
    return if (readResult is RemoteSettingsResult.DiskFailure) {
        fetch()
    } else {
        readResult
    }
}

/**
 * Fetches files from remote servers. If file retrieved successfully, updates the local storage.
 */
suspend fun RemoteSettingsClient.fetchAndWrite(): RemoteSettingsResult {
    val fetchResult = fetch()
    return if (fetchResult is RemoteSettingsResult.Success) {
        write(fetchResult.response)
    } else {
        fetchResult
    }
}

/**
 * Base class for different result states of remote settings operations.
 */
sealed class RemoteSettingsResult {

    /**
     * Represents a successful outcome of a remote settings operation.
     */
    data class Success(val response: RemoteSettingsResponse) : RemoteSettingsResult()

    /**
     * Represents a failure due to issues related to local storage (e.g., file not found, file format error).
     */
    data class DiskFailure(val error: Exception) : RemoteSettingsResult()

    /**
     * Represents a failure due to network-related issues during remote settings operation
     * (e.g., network not available, server error).
     */
    data class NetworkFailure(val error: Exception) : RemoteSettingsResult()
}

/**
 * Data class representing serializable version of RemoteSettingsResponse.
 */
@Serializable
private data class SerializableRemoteSettingsResponse(
    val records: List<SerializableRemoteSettingsRecord>,
    val lastModified: ULong,
)

/**
 * Data class representing serializable version of RemoteSettingsRecord.
 */
@Serializable
private data class SerializableRemoteSettingsRecord(
    val id: String,
    val lastModified: ULong,
    val deleted: Boolean,
    val attachment: SerializableAttachment?,
    @Serializable(with = JSONObjectSerializer::class)
    val fields: JSONObject,
)

/**
 * Data class representing serializable version of (RemoteSettings) Attachment.
 */
@Serializable
private data class SerializableAttachment(
    val filename: String,
    val mimetype: String,
    val location: String,
    val hash: String,
    val size: ULong,
)

private object JSONObjectSerializer : KSerializer<JSONObject> {
    override val descriptor: SerialDescriptor = PrimitiveSerialDescriptor("JSONObject", PrimitiveKind.STRING)

    override fun serialize(encoder: Encoder, value: JSONObject) {
        encoder.encodeString(value.toString())
    }

    override fun deserialize(decoder: Decoder): JSONObject {
        val jsonString = decoder.decodeString()
        return JSONObject(jsonString)
    }
}

private fun RemoteSettingsRecord.toSerializable(): SerializableRemoteSettingsRecord {
    return SerializableRemoteSettingsRecord(
        id = this.id,
        lastModified = this.lastModified,
        deleted = this.deleted,
        attachment = this.attachment?.toSerializable(),
        fields = this.fields,
    )
}

private fun RemoteSettingsResponse.toSerializable(): SerializableRemoteSettingsResponse {
    return SerializableRemoteSettingsResponse(
        records = this.records.map { it.toSerializable() },
        lastModified = this.lastModified,
    )
}

private fun Attachment.toSerializable(): SerializableAttachment {
    return SerializableAttachment(
        filename = this.filename,
        mimetype = this.mimetype,
        location = this.location,
        hash = this.hash,
        size = this.size,
    )
}

private fun SerializableRemoteSettingsResponse.toRemoteSettingsResponse(): RemoteSettingsResponse {
    return RemoteSettingsResponse(
        records = this.records.map { it.toRemoteSettingsRecord() },
        lastModified = this.lastModified,
    )
}

private fun SerializableRemoteSettingsRecord.toRemoteSettingsRecord(): RemoteSettingsRecord {
    return RemoteSettingsRecord(
        id = this.id,
        lastModified = this.lastModified,
        deleted = this.deleted,
        attachment = this.attachment?.toAttachment(),
        fields = this.fields,
    )
}

private fun SerializableAttachment.toAttachment(): Attachment {
    return Attachment(
        filename = this.filename,
        mimetype = this.mimetype,
        location = this.location,
        hash = this.hash,
        size = this.size,
    )
}
