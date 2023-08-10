/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.remotesettings

import android.util.AtomicFile
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.appservices.remotesettings.Attachment
import mozilla.appservices.remotesettings.RemoteSettings
import mozilla.appservices.remotesettings.RemoteSettingsConfig
import mozilla.appservices.remotesettings.RemoteSettingsException
import mozilla.appservices.remotesettings.RemoteSettingsRecord
import mozilla.appservices.remotesettings.RsJsonObject
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.util.writeString
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.FileNotFoundException
import java.io.FileReader
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
    private val collectionName: String = "",
) {

    private val config = RemoteSettingsConfig(
        serverUrl = serverUrl,
        bucketName = bucketName,
        collectionName = collectionName,
    )
    private val serverHostName = URL(serverUrl).host
    private val path = "${storageRootDirectory.path}/$serverHostName/$bucketName/$collectionName"
    private val file = File(path)

    /**
     * Fetches all records for the collection from remote servers.
     */
    suspend fun fetch(): RemoteSettingsResult = withContext(Dispatchers.IO) {
        try {
            val serverRecords = RemoteSettings(config).use {
                it.getRecords().records
            }
            RemoteSettingsResult.Success(serverRecords)
        } catch (e: RemoteSettingsException) {
            Logger.error(e.message.toString())
            RemoteSettingsResult.NetworkFailure(e)
        }
    }

    /**
     * Updates the local storage with [records] data.
     */
    suspend fun write(records: List<RemoteSettingsRecord>): RemoteSettingsResult = withContext(Dispatchers.IO) {
        try {
            if (records.isEmpty()) {
                return@withContext RemoteSettingsResult.DiskFailure(IllegalArgumentException("Cannot write empty list"))
            }
            val jsonArray = JSONArray()
            records.forEach {
                val recordJson = JSONObject().apply {
                    put("id", it.id)
                    put("lastModified", it.lastModified.toString())
                    put("deleted", it.deleted)
                    put("attachment", it.attachment)
                    put("fields", it.fields)
                }
                jsonArray.put(recordJson)
            }

            val jsonObject = JSONObject()
            jsonObject.put("records", jsonArray)
            AtomicFile(file).writeString { jsonObject.toString() }
            RemoteSettingsResult.Success(records)
        } catch (e: IOException) {
            RemoteSettingsResult.DiskFailure(e)
        }
    }

    /**
     * Reads all records for a collection  found in the local storage.
     */
    suspend fun read(): RemoteSettingsResult = withContext(Dispatchers.IO) {
        if (!file.exists()) {
            RemoteSettingsResult.DiskFailure(FileNotFoundException("File not found"))
        } else {
            try {
                val fileReader = FileReader(file)
                val jsonObject = JSONObject(fileReader.readText())
                val jsonArray = jsonObject.getJSONArray("records")
                val records = mutableListOf<RemoteSettingsRecord>()

                for (i in 0 until jsonArray.length()) {
                    val recordJson = jsonArray.getJSONObject(i)
                    val id = recordJson.getString("id")
                    val lastModified = recordJson.getString("lastModified").toULong()
                    val deleted = recordJson.getBoolean("deleted")
                    val attachment = recordJson.opt("attachment")
                    val fields = recordJson.opt("fields")

                    records.add(
                        RemoteSettingsRecord(
                            id = id,
                            lastModified = lastModified,
                            deleted = deleted,
                            attachment = attachment as Attachment?,
                            fields = fields as RsJsonObject,
                        ),
                    )
                }
                fileReader.close()
                RemoteSettingsResult.Success(records)
            } catch (e: IOException) {
                RemoteSettingsResult.DiskFailure(e)
            }
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
        write(fetchResult.records)
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
    data class Success(val records: List<RemoteSettingsRecord>) : RemoteSettingsResult()

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
