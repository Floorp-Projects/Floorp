/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard.storage.flatfile

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.service.fretboard.Experiment
import mozilla.components.service.fretboard.ExperimentsSnapshot
import mozilla.components.support.test.robolectric.testContext
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File
import java.nio.file.Files
import java.nio.file.Paths

@RunWith(AndroidJUnit4::class)
class FlatFileExperimentStorageTest {

    @Test
    fun save() {
        val experiments = listOf(
            Experiment("sample-id",
                "sample-name",
                "sample-description",
                Experiment.Matcher("es|en", "sample-appId", listOf("US")),
                Experiment.Bucket(20, 0),
                1526991669)
        )
        val file = File(testContext.filesDir, "experiments.json")
        assertFalse(file.exists())
        FlatFileExperimentStorage(file).save(ExperimentsSnapshot(experiments, 1526991669))
        assertTrue(file.exists())
        val experimentsJson = JSONObject(String(Files.readAllBytes(Paths.get(file.absolutePath))))
        checkSavedExperimentsJson(experimentsJson)
        file.delete()
    }

    @Test
    fun replacingContent() {
        val file = File(testContext.filesDir, "experiments.json")

        FlatFileExperimentStorage(file).save(ExperimentsSnapshot(listOf(
            Experiment("sample-id",
                "sample-name",
                "sample-description",
                Experiment.Matcher(),
                Experiment.Bucket(20, 0),
                1526991669)
        ), 1526991669))

        FlatFileExperimentStorage(file).save(ExperimentsSnapshot(listOf(
            Experiment("sample-id-updated",
                "sample-name-updated",
                "sample-description-updated",
                Experiment.Matcher(),
                Experiment.Bucket(100, 10),
                1526991700)
        ), 1526991700))

        val loadedExperiments = FlatFileExperimentStorage(file).retrieve().experiments

        assertEquals(1, loadedExperiments.size)

        val loadedExperiment = loadedExperiments[0]

        assertEquals("sample-id-updated", loadedExperiment.id)
        assertEquals("sample-name-updated", loadedExperiment.name)
        assertEquals("sample-description-updated", loadedExperiment.description)
        assertEquals(10, loadedExperiment.bucket!!.min)
        assertEquals(100, loadedExperiment.bucket!!.max)
        assertEquals(1526991700L, loadedExperiment.lastModified)
    }

    private fun checkSavedExperimentsJson(experimentsJson: JSONObject) {
        assertEquals(2, experimentsJson.length())
        val experimentsJsonArray = experimentsJson.getJSONArray("experiments")
        assertEquals(1, experimentsJsonArray.length())
        val experimentJson = experimentsJsonArray[0] as JSONObject
        assertEquals("sample-name", experimentJson.getString("name"))
        val experimentMatch = experimentJson.getJSONObject("match")
        assertEquals("es|en", experimentMatch.getString("lang"))
        assertEquals("sample-appId", experimentMatch.getString("appId"))
        val experimentRegions = experimentMatch.getJSONArray("regions")
        assertEquals(1, experimentRegions.length())
        assertEquals("US", experimentRegions[0])
        val experimentBuckets = experimentJson.getJSONObject("buckets")
        assertEquals(20, experimentBuckets.getInt("max"))
        assertEquals(0, experimentBuckets.getInt("min"))
        assertEquals("sample-description", experimentJson.getString("description"))
        assertEquals("sample-id", experimentJson.getString("id"))
        assertEquals(1526991669L, experimentJson.getLong("last_modified"))
    }

    @Test
    fun retrieve() {
        val file = File(testContext.filesDir, "experiments.json")
        file.writer().use {
            it.write("""{"experiments":[{"name":"sample-name","match":{"lang":"es|en","appId":"sample-appId","regions":["US"]},"buckets":{"max":20,"min":0},"description":"sample-description","id":"sample-id","last_modified":1526991669}],"last_modified":1526991669}""")
        }
        val experimentsResult = FlatFileExperimentStorage(file).retrieve()
        val experiments = experimentsResult.experiments
        file.delete()
        assertEquals(1, experiments.size)
        assertEquals("sample-id", experiments[0].id)
        assertEquals("sample-description", experiments[0].description)
        assertEquals("sample-name", experiments[0].name)
        assertEquals(listOf("US"), experiments[0].match?.regions)
        assertEquals("es|en", experiments[0].match?.language)
        assertEquals("sample-appId", experiments[0].match?.appId)
        assertEquals(20, experiments[0].bucket?.max)
        assertEquals(0, experiments[0].bucket?.min)
        assertEquals(1526991669L, experiments[0].lastModified)
        assertEquals(1526991669L, experimentsResult.lastModified)
    }

    @Test
    fun retrieveFileNotFound() {
        val file = File(testContext.filesDir, "missingFile.json")
        val experimentsResult = FlatFileExperimentStorage(file).retrieve()
        assertEquals(0, experimentsResult.experiments.size)
        assertNull(experimentsResult.lastModified)
    }

    @Test
    fun readingCorruptJSON() {
        val file = File(testContext.filesDir, "corrupt-experiments.json")
        file.writer().use {
            it.write("""{"experiment":[""")
        }

        val snapshot = FlatFileExperimentStorage(file).retrieve()

        assertNull(snapshot.lastModified)
        assertEquals(0, snapshot.experiments.size)
    }
}