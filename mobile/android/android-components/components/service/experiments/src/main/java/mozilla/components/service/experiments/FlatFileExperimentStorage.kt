/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

import android.util.AtomicFile
import mozilla.components.support.base.log.logger.Logger
import org.json.JSONException
import java.io.FileNotFoundException
import java.io.File
import java.io.IOException

/**
 * Class which uses a flat JSON file as an experiment storage mechanism
 *
 * @param file file where to store experiments
 */
internal class FlatFileExperimentStorage(file: File) {
    private val atomicFile: AtomicFile = AtomicFile(file)
    private val logger: Logger = Logger(LOG_TAG)

    fun retrieve(): ExperimentsSnapshot {
        return try {
            val experimentsJson = String(atomicFile.readFully())
            ExperimentsSerializer().fromJson(experimentsJson)
        } catch (e: FileNotFoundException) {
            logger.error("Caught error trying to retrieve experiments from storage: $e")
            ExperimentsSnapshot(listOf(), null)
        } catch (e: JSONException) {
            // The JSON we read from disk is corrupt. There's nothing we can do here and therefore
            // we just continue as if the file wouldn't exist.
            logger.error("Caught error trying to retrieve experiments from storage: $e")
            ExperimentsSnapshot(listOf(), null)
        }
    }

    fun save(snapshot: ExperimentsSnapshot) {
        val experimentsJson = ExperimentsSerializer().toJson(snapshot)

        val stream = atomicFile.startWrite()
        try {
            stream.writer().use {
                it.append(experimentsJson)
            }

            atomicFile.finishWrite(stream)
        } catch (e: IOException) {
            atomicFile.failWrite(stream)
        }
    }

    fun delete() {
        atomicFile.delete()
    }

    companion object {
        private const val LOG_TAG = "experiments"
    }
}
