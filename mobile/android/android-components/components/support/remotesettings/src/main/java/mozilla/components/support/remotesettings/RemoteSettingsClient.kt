/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.remotesettings

import android.util.AtomicFile
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.appservices.remotesettings.RemoteSettings
import mozilla.appservices.remotesettings.RemoteSettingsConfig
import mozilla.appservices.remotesettings.RemoteSettingsException
import mozilla.appservices.remotesettings.RemoteSettingsResponse
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.util.writeString
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
        }
    }

    /**
     * Updates the local storage with [response] data.
     * The full response is required because the `lastModified` property may be used for future comparisons.
     */
    suspend fun write(response: RemoteSettingsResponse): RemoteSettingsResult = withContext(Dispatchers.IO) {
        try {
            val jsonObject = JSONObject()
            jsonObject.put("response", response)
            AtomicFile(file).writeString { jsonObject.toString() }
            RemoteSettingsResult.Success(response)
        } catch (e: IOException) {
            RemoteSettingsResult.DiskFailure(e)
        }
    }

    /**
     * Updates the local storage with [response] data if [response.lastModified] is more recent than [lastModified]
     *
     * @param response is collections data from remote server or local storage.
     * @param lastModified tells when the last change was made to the response.
     */
    suspend fun writeIfModifiedSince(
        response: RemoteSettingsResponse,
        lastModified: ULong,
    ): RemoteSettingsResult =
        withContext(Dispatchers.IO) {
            try {
                if (response.lastModified > lastModified) {
                    write(response)
                } else {
                    RemoteSettingsResult.Success(response)
                }
            } catch (e: IOException) {
                RemoteSettingsResult.DiskFailure(e)
            }
        }

    /**
     * Reads all response for a collection found in the local storage.
     */
    suspend fun read(): RemoteSettingsResult = withContext(Dispatchers.IO) {
        if (!file.exists()) {
            RemoteSettingsResult.DiskFailure(FileNotFoundException("File not found"))
        } else {
            try {
                val fileReader = FileReader(file)
                val jsonObject = JSONObject(fileReader.readText())
                val response = jsonObject.get("response") as RemoteSettingsResponse

                fileReader.close()
                RemoteSettingsResult.Success(response)
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
        write(fetchResult.response)
    } else {
        fetchResult
    }
}

/**
 * Fetches files from remote servers.
 * If file retrieved successfully and last modified changed, updates the local storage.
 */
suspend fun RemoteSettingsClient.fetchAndWriteIfResponseUpdated(lastModified: ULong): RemoteSettingsResult {
    val fetchResult = fetch()
    return if (fetchResult is RemoteSettingsResult.Success) {
        writeIfModifiedSince(fetchResult.response, lastModified)
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
