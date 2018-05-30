/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard.storage.atomicfile

import android.support.test.InstrumentationRegistry
import android.support.test.runner.AndroidJUnit4
import android.util.AtomicFile
import mozilla.components.service.fretboard.Experiment
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File

@RunWith(AndroidJUnit4::class)
class AtomicFileExperimentStorageTest {
    @Test
    fun testSave() {
        val experiments = listOf(
            Experiment("sample-id",
                "sample-name",
                "sample-description",
                Experiment.Matcher("es|en", "sample-appId", listOf("US")),
                Experiment.Bucket(20, 0),
                1526991669)
        )
        val atomicFile = AtomicFile(File(InstrumentationRegistry.getContext().filesDir, "experiments.json"))
        assertFalse(atomicFile.baseFile.exists())
        AtomicFileExperimentStorage(atomicFile).save(experiments)
        assertTrue(atomicFile.baseFile.exists())
        assertEquals("""{"experiments":[{"name":"sample-name","match":{"lang":"es|en","appId":"sample-appId","regions":["US"]},"buckets":{"max":20,"min":0},"description":"sample-description","id":"sample-id","last_modified":1526991669}]}""", String(atomicFile.readFully()))
        atomicFile.delete()
    }

    @Test
    fun testRetrieve() {
        val file = File(InstrumentationRegistry.getContext().filesDir, "experiments.json")
        file.writer().use {
            it.write("""{"experiments":[{"name":"sample-name","match":{"lang":"es|en","appId":"sample-appId","regions":["US"]},"buckets":{"max":20,"min":0},"description":"sample-description","id":"sample-id","last_modified":1526991669}]}""")
        }
        val experiments = AtomicFileExperimentStorage(AtomicFile(file)).retrieve()
        assertEquals(1, experiments.size)
        assertEquals("sample-id", experiments[0].id)
        assertEquals("sample-description", experiments[0].description)
        assertEquals("sample-name", experiments[0].name)
        assertEquals(listOf("US"), experiments[0].match?.regions)
        assertEquals("es|en", experiments[0].match?.language)
        assertEquals("sample-appId", experiments[0].match?.appId)
        assertEquals(20, experiments[0].bucket?.max)
        assertEquals(0, experiments[0].bucket?.min)
        assertEquals(1526991669, experiments[0].lastModified)
    }

    @Test
    fun testRetrieveFileNotFound() {
        val file = File(InstrumentationRegistry.getContext().filesDir, "missingFile.json")
        val experiments = AtomicFileExperimentStorage(AtomicFile(file)).retrieve()
        assertEquals(0, experiments.size)
    }
}