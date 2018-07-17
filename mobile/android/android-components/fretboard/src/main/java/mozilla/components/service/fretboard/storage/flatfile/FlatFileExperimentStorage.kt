/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard.storage.flatfile

import android.util.AtomicFile
import mozilla.components.service.fretboard.Experiment
import mozilla.components.service.fretboard.ExperimentStorage
import java.io.FileNotFoundException
import java.io.File

class FlatFileExperimentStorage(file: File) : ExperimentStorage {
    private val atomicFile: AtomicFile = AtomicFile(file)

    override fun retrieve(): List<Experiment> {
        try {
            val experimentsJson = String(atomicFile.readFully())
            return ExperimentsSerializer().fromJson(experimentsJson)
        } catch (e: FileNotFoundException) {
            return listOf()
        }
    }

    override fun save(experiments: List<Experiment>) {
        val experimentsJson = ExperimentsSerializer().toJson(experiments)
        atomicFile.startWrite().writer().use {
            it.append(experimentsJson)
        }
    }
}
