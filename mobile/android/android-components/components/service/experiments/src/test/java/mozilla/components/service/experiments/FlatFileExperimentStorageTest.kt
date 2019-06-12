/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.ktx.android.org.json.toList
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
    private var currentTime = System.currentTimeMillis() / 1000
    private var pastTime = currentTime - 1000

    private fun checkSavedExperimentsJson(experimentsJson: JSONObject) {
        assertEquals(2, experimentsJson.length())
        val experimentsJsonArray = experimentsJson.getJSONArray("experiments")
        assertEquals(1, experimentsJsonArray.length())
        val experiment = experimentsJsonArray[0] as JSONObject

        assertEquals("sample-id", experiment.getString("id"))
        assertEquals("sample-description", experiment.getString("description"))
        assertEquals(pastTime, experiment.getLong("last_modified"))

        val buckets = experiment.getJSONObject("buckets")
        assertEquals(0, buckets.getInt("start"))
        assertEquals(20, buckets.getInt("count"))

        val branches = experiment.getJSONArray("branches")
        assertEquals(2, branches.length())
        val branch1 = branches.getJSONObject(0)
        assertEquals("branch1", branch1.getString("name"))
        assertEquals(2, branch1.getInt("ratio"))
        val branch2 = branches.getJSONObject(1)
        assertEquals("branch2", branch2.getString("name"))
        assertEquals(5, branch2.getInt("ratio"))

        val match = experiment.getJSONObject("match")
        assertEquals("es|en", match.getString("locale_language"))
        assertEquals("sample-appId", match.getString("app_id"))
        val regions = match.getJSONArray("regions")
        assertEquals(listOf("US"), regions.toList<String>())
    }

    @Test
    fun save() {
        val experiments = listOf(
            createDefaultExperiment(
                id = "sample-id",
                description = "sample-description",
                match = createDefaultMatcher(
                    localeLanguage = "es|en",
                    appId = "sample-appId",
                    regions = listOf("US")
                ),
                buckets = Experiment.Buckets(0, 20),
                branches = listOf(
                    Experiment.Branch("branch1", 2),
                    Experiment.Branch("branch2", 5)
                ),
                lastModified = pastTime
            )
        )
        val file = File(testContext.filesDir, "experiments.json")
        assertFalse(file.exists())
        FlatFileExperimentStorage(file).save(ExperimentsSnapshot(experiments, pastTime))
        assertTrue(file.exists())
        val experimentsJson = JSONObject(String(Files.readAllBytes(Paths.get(file.absolutePath))))
        checkSavedExperimentsJson(experimentsJson)
        file.delete()
    }

    @Test
    fun replacingContent() {
        val file = File(testContext.filesDir, "experiments.json")

        FlatFileExperimentStorage(file).save(ExperimentsSnapshot(listOf(
            createDefaultExperiment(
                id = "sample-id",
                description = "sample-description",
                buckets = Experiment.Buckets(0, 20),
                branches = listOf(
                    Experiment.Branch("branch1", 2),
                    Experiment.Branch("branch2", 5)
                ),
                lastModified = pastTime
            )
        ), pastTime))

        FlatFileExperimentStorage(file).save(ExperimentsSnapshot(listOf(
            createDefaultExperiment(
                id = "sample-id-updated",
                description = "sample-description-updated",
                buckets = Experiment.Buckets(10, 100),
                branches = listOf(
                    Experiment.Branch("some-branch", 42)
                ),
                lastModified = currentTime
            )
        ), currentTime))

        val loadedExperiments = FlatFileExperimentStorage(file).retrieve().experiments

        assertEquals(1, loadedExperiments.size)

        val loadedExperiment = loadedExperiments[0]

        assertEquals("sample-id-updated", loadedExperiment.id)
        assertEquals("sample-description-updated", loadedExperiment.description)
        assertEquals(currentTime, loadedExperiment.lastModified)
        assertEquals(10, loadedExperiment.buckets.start)
        assertEquals(100, loadedExperiment.buckets.count)
        assertEquals(100, loadedExperiment.buckets.count)
        assertEquals(1, loadedExperiment.branches.size)
        assertEquals("some-branch", loadedExperiment.branches[0].name)
        assertEquals(42, loadedExperiment.branches[0].ratio)
    }

    @Test
    fun retrieve() {
        val file = File(testContext.filesDir, "experiments.json")
        val json = """
            {
              "experiments": [
                {
                  "id": "sample-id",
                  "match": {
                    "locale_language": "es|en",
                    "app_id": "sample-appId",
                    "regions": [
                      "US"
                    ]
                  },
                  "buckets": {
                    "start": 0,
                    "count": 20
                  },
                  "branches": [
                    {
                      "name": "some-branch",
                      "ratio": 42
                    }
                  ],
                  "description": "sample-description",
                  "last_modified": $pastTime
                }
              ],
              "last_modified": $pastTime
            }
        """.trimIndent()

        file.writer().use {
            it.write(json)
        }
        val experimentsResult = FlatFileExperimentStorage(file).retrieve()
        val experiments = experimentsResult.experiments
        file.delete()
        assertEquals(1, experiments.size)

        val experiment = experiments[0]
        assertEquals("sample-id", experiment.id)
        assertEquals("sample-description", experiment.description)
        assertEquals(listOf("US"), experiment.match.regions)
        assertEquals("es|en", experiment.match.localeLanguage)
        assertEquals("sample-appId", experiment.match.appId)
        assertEquals(0, experiment.buckets.start)
        assertEquals(20, experiment.buckets.count)
        assertEquals(pastTime, experiment.lastModified)
        assertEquals(1, experiment.branches.size)
        assertEquals("some-branch", experiment.branches[0].name)
        assertEquals(42, experiment.branches[0].ratio)

        assertEquals(pastTime, experimentsResult.lastModified)
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