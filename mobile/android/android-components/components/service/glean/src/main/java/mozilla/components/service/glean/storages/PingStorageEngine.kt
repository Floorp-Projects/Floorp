/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.content.Context
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.Dispatchers as KotlinDispatchers
import kotlinx.coroutines.launch
import mozilla.components.service.glean.Glean
import mozilla.components.service.glean.config.Configuration
import mozilla.components.service.glean.utils.ensureDirectoryExists
import mozilla.components.support.base.log.logger.Logger
import java.io.BufferedReader
import java.io.File
import java.io.FileNotFoundException
import java.io.FileOutputStream
import java.io.FileReader
import java.io.IOException
import java.util.UUID

/**
 * This class implementation stores pings as files on disk by serializing the upload path and the
 * JSON ping payload data.  Data is stored in the application's data directory under the
 * [Glean.GLEAN_DATA_DIR] in a folder called pending_pings, which is consistent with desktop.
 *
 * The pings are further separated into folders based on the ping name in order to make it easier to
 * upload different pings on different schedules.
 */
internal class PingStorageEngine(context: Context) {

    private val logger: Logger = Logger("glean/PingStorageEngine")

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    val storageDirectory: File = File(context.applicationInfo.dataDir, PING_STORAGE_DIRECTORY)

    // The cache directory is used to temporarily write ping files before they are moved
    // to the final [PING_STORAGE_DIRECTORY]. This directory is still app-private, so
    // saving pings there it's still safe.
    private val cacheDirectory: File = context.cacheDir

    companion object {
        // Since ping file names are UUIDs, this matches UUIDs for filtering purposes
        private const val FILE_PATTERN =
            "[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}"
        // Base directory for storing serialized pings
        private const val PING_STORAGE_DIRECTORY = "${Glean.GLEAN_DATA_DIR}/pending_pings"
    }

    /**
     * Function to store the ping to file to the application's storage directory
     *
     * @param uuidFileName UUID that will represent the file named used to store the ping
     * @param uploadPath String representing the upload path used for uploading the ping
     * @param pingData Serialized JSON string representing the ping payload
     */
    fun store(uuidFileName: UUID, uploadPath: String, pingData: String): Job {
        return GlobalScope.launch(KotlinDispatchers.IO) {
            // Check that the director exists and create it if needed
            ensureDirectoryExists(storageDirectory)

            // Build the file path for the ping, using the UUID from the path for the file name
            val pingFile = File(storageDirectory, uuidFileName.toString())
            logger.debug("Storing ping $uuidFileName at ${pingFile.absolutePath}")

            // Write ping to file
            writePingToFile(pingFile, uploadPath, pingData)
        }
    }

    /**
     * Function to deserialize and process all serialized ping files.  This function will ignore
     * files that don't match the UUID regex and just delete them to prevent files from polluting
     * the ping storage directory.
     *
     * @param processingCallback Callback function to do the actual process action on the ping.
     *                           Typically this will be a [PingUploader.upload] function.
     * @return Boolean representing the success of the upload task. This may be the value bubbled up
     *         from the callback, or if there was an error reading the files.
     */
    @Synchronized
    fun process(processingCallback: (String, String, Configuration) -> Boolean): Boolean {
        // Marked as @Synchronized so that this doesn't run at the same time as
        // clearPendingPings.
        logger.debug("Processing persisted pings at ${storageDirectory.absolutePath}")

        var success = true

        storageDirectory.listFiles()?.forEach { file ->
            if (file.name.matches(Regex(FILE_PATTERN))) {
                logger.debug("Processing ping: ${file.name}")
                if (!processFile(file, processingCallback)) {
                    logger.error("Error processing ping file: ${file.name}")
                    success = false
                }
            } else {
                // Delete files that don't match the UUID FILE_PATTERN regex
                logger.debug("Pattern mismatch. Deleting ${file.name}")
                file.delete()
            }
        }

        return success
    }

    /**
     * This function encapsulates processing of a single ping file.
     *
     * @param file The [File] to process
     * @param processingCallback the callback that actually processes the file
     *
     */
    private fun processFile(
        file: File,
        processingCallback: (String, String, Configuration) -> Boolean
    ): Boolean {
        var processed = false
        BufferedReader(FileReader(file)).use {
            try {
                val path = it.readLine()
                val serializedPing = it.readLine()

                processed = serializedPing == null ||
                    processingCallback(path, serializedPing, Glean.configuration)
            } catch (e: FileNotFoundException) {
                // This shouldn't happen after we queried the directory.
                logger.error("Could not find ping file ${file.name}")
                return false
            } catch (e: IOException) {
                // Something is not right.
                logger.error("IO Exception when reading file ${file.name}")
                return false
            }
        }

        return if (processed) {
            val fileWasDeleted = file.delete()
            logger.debug("${file.name} was deleted: $fileWasDeleted")
            true
        } else {
            // The callback couldn't process this file.
            false
        }
    }

    /**
     * Serializes the upload path and data to a file to persist data for the upload worker to use.
     *
     * @param pingFile The file to write the path and data to
     * @param uploadPath The upload path to serialize
     * @param pingData The ping data to serialize
     */
    private fun writePingToFile(pingFile: File, uploadPath: String, pingData: String) {
        // Store data to a temporary file.
        val temporaryFile = File.createTempFile("glean-ping", ".tmp", cacheDirectory)

        // Write the ping content data to the temp file.
        FileOutputStream(temporaryFile, true).bufferedWriter().use {
            try {
                it.write(uploadPath)
                it.newLine()
                it.write(pingData)
                it.newLine()
                it.flush()
            } catch (e: IOException) {
                logger.warn("IOException while writing ping to file", e)
                return
            }
        }

        // Nothing weird happened, move the file to the final location. This could still
        // fail, but there's really no way for us to do something about it.
        if (!temporaryFile.renameTo(pingFile)) {
            logger.warn("Unable to move ${temporaryFile.absolutePath} to ${pingFile.absolutePath}")
        }
    }

    /**
     * Deletes any pending pings on disk.
     */
    @Synchronized
    internal fun clearPendingPings() {
        // Marked as @Synchronized so that this doesn't run at the same time as
        // process.
        storageDirectory.listFiles()?.forEach { file ->
            val fileWasDeleted = file.delete()
            logger.debug("${file.name} was deleted: $fileWasDeleted")
        }
    }

    /**
     * Returns the number of pending pings on disk, for testing purposes.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    @Synchronized
    internal fun testGetNumPendingPings(): Int {
        return storageDirectory.listFiles()!!.size
    }
}
