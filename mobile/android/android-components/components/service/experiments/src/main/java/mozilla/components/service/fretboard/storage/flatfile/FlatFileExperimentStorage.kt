/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard.storage.flatfile

import android.util.AtomicFile
import mozilla.components.service.fretboard.ExperimentStorage
import mozilla.components.service.fretboard.ExperimentsSnapshot
import org.json.JSONException
import java.io.FileNotFoundException
import java.io.File
import java.io.IOException

/**
 * Class which uses a flat JSON file as an experiment storage mechanism
 *
 * @param file file where to store experiments
 */
class FlatFileExperimentStorage(file: File) : ExperimentStorage {
    private val atomicFile: AtomicFile = AtomicFile(file)

    override fun retrieve(): ExperimentsSnapshot {
        return try {
            val experimentsJson = String(atomicFile.readFully())
            ExperimentsSerializer().fromJson(experimentsJson)
        } catch (e: FileNotFoundException) {
            ExperimentsSnapshot(listOf(), null)
        } catch (e: JSONException) {
            // The JSON we read from disk is corrupt. There's nothing we can do here and therefore
            // we just continue as if the file wouldn't exist.
            ExperimentsSnapshot(listOf(), null)
        }
    }

    override fun save(snapshot: ExperimentsSnapshot) {
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
}
