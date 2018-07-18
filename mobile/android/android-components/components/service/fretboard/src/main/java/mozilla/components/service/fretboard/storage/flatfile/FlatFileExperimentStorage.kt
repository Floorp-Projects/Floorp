/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard.storage.flatfile

import android.util.AtomicFile
import mozilla.components.service.fretboard.ExperimentStorage
import mozilla.components.service.fretboard.ExperimentsSnapshot
import java.io.FileNotFoundException
import java.io.File

class FlatFileExperimentStorage(file: File) : ExperimentStorage {
    private val atomicFile: AtomicFile = AtomicFile(file)

    override fun retrieve(): ExperimentsSnapshot {
        try {
            val experimentsJson = String(atomicFile.readFully())
            return ExperimentsSerializer().fromJson(experimentsJson)
        } catch (e: FileNotFoundException) {
            return ExperimentsSnapshot(listOf(), null)
        }
    }

    override fun save(snapshot: ExperimentsSnapshot) {
        val experimentsJson = ExperimentsSerializer().toJson(snapshot)
        atomicFile.startWrite().writer().use {
            it.append(experimentsJson)
        }
    }
}
