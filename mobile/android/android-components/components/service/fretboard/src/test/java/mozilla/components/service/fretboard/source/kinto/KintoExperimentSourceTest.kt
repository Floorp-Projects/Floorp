/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard.source.kinto

import mozilla.components.service.fretboard.Experiment
import mozilla.components.service.fretboard.ExperimentsSnapshot
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.robolectric.RobolectricTestRunner
import java.net.URL

@RunWith(RobolectricTestRunner::class)
class KintoExperimentSourceTest {
    private val baseUrl = "http://mydomain.test"
    private val bucketName = "fretboard"
    private val collectionName = "experiments"

    @Test
    fun testNoExperiments() {
        val httpClient = mock(HttpClient::class.java)

        `when`(httpClient.get(URL("$baseUrl/buckets/$bucketName/collections/$collectionName/records")))
            .thenReturn("""{"data":[]}""")
        val experimentSource = KintoExperimentSource(baseUrl, bucketName, collectionName, httpClient)
        val result = experimentSource.getExperiments(ExperimentsSnapshot(listOf(), null))
        assertEquals(0, result.experiments.size)
        assertNull(result.lastModified)
    }

    @Test
    fun testGetExperimentsNoDiff() {
        val httpClient = mock(HttpClient::class.java)

        `when`(httpClient.get(URL("$baseUrl/buckets/$bucketName/collections/$collectionName/records")))
            .thenReturn("""{"data":[{"name":"first-name","match":{"lang":"eng","appId":"first-appId",regions:[]},"schema":1523549592861,"buckets":{"max":"100","min":"0"},"description":"first-description", "id":"first-id","last_modified":1523549895713}]}""")

        val expectedExperiment = Experiment("first-id",
            "first-name",
            "first-description",
            Experiment.Matcher("eng", "first-appId", listOf()),
            Experiment.Bucket(100, 0),
            1523549895713
        )

        val experimentSource = KintoExperimentSource(baseUrl, bucketName, collectionName, httpClient)
        val kintoExperiments = experimentSource.getExperiments(ExperimentsSnapshot(listOf(), null))
        assertEquals(1, kintoExperiments.experiments.size)
        assertEquals(expectedExperiment, kintoExperiments.experiments[0])
        assertEquals(1523549895713, kintoExperiments.lastModified)
    }

    @Test
    fun testGetExperimentsDiffAdd() {
        val httpClient = mock(HttpClient::class.java)
        `when`(httpClient.get(URL("$baseUrl/buckets/$bucketName/collections/$collectionName/records?_since=1523549890000")))
            .thenReturn("""{"data":[{"name":"first-name","match":{"lang":"eng","appId":"first-appId",regions:[]},"schema":1523549592861,"buckets":{"max":"100","min":"0"},"description":"first-description", "id":"first-id","last_modified":1523549895713}]}""")

        val kintoExperiment = Experiment("first-id",
            "first-name",
            "first-description",
            Experiment.Matcher("eng", "first-appId", listOf()),
            Experiment.Bucket(100, 0),
            1523549895713
        )

        val storageExperiment = Experiment("id",
            "name",
            "description",
            Experiment.Matcher("eng", "appId", listOf("US")),
            Experiment.Bucket(10, 5),
            1523549890000
        )

        val experimentSource = KintoExperimentSource(baseUrl, bucketName, collectionName, httpClient)
        val kintoExperiments = experimentSource.getExperiments(ExperimentsSnapshot(listOf(storageExperiment), 1523549890000))
        assertEquals(2, kintoExperiments.experiments.size)
        assertEquals(storageExperiment, kintoExperiments.experiments[0])
        assertEquals(kintoExperiment, kintoExperiments.experiments[1])
        assertEquals(1523549895713, kintoExperiments.lastModified)
    }

    @Test
    fun testGetExperimentsDiffDelete() {
        val httpClient = mock(HttpClient::class.java)
        `when`(httpClient.get(URL("$baseUrl/buckets/$bucketName/collections/$collectionName/records?_since=1523549890000")))
            .thenReturn("""{"data":[{"deleted":true,"id":"id","last_modified":1523549899999}]}""")
        `when`(httpClient.get(URL("$baseUrl/buckets/$bucketName/collections/$collectionName/records?_since=1523549899999")))
            .thenReturn("""{"data":[]}""")

        val storageExperiment = Experiment("id",
            "name",
            "description",
            Experiment.Matcher("eng", "appId", listOf("US")),
            Experiment.Bucket(10, 5),
            1523549890000
        )

        val secondExperiment = Experiment("id2",
            "name2",
            "description2",
            Experiment.Matcher("eng", "appId", listOf("US")),
            Experiment.Bucket(10, 5),
            1523549890000)

        val experimentSource = KintoExperimentSource(baseUrl, bucketName, collectionName, httpClient)
        val kintoExperiments = experimentSource.getExperiments(ExperimentsSnapshot(listOf(storageExperiment, secondExperiment), 1523549890000))
        assertEquals(1, kintoExperiments.experiments.size)
        assertEquals(1523549899999, kintoExperiments.lastModified)
        val experimentsResult = experimentSource.getExperiments(ExperimentsSnapshot(kintoExperiments.experiments, 1523549899999))
        assertEquals(1, experimentsResult.experiments.size)
        assertEquals(1523549899999, experimentsResult.lastModified)
    }

    @Test
    fun testGetExperimentsDiffUpdate() {
        val httpClient = mock(HttpClient::class.java)
        `when`(httpClient.get(URL("$baseUrl/buckets/$bucketName/collections/$collectionName/records?_since=1523549800000")))
            .thenReturn("""{"data":[{"name":"first-name","match":{"lang":"eng","appId":"first-appId",regions:[]},"schema":1523549592861,"buckets":{"max":"100","min":"0"},"description":"first-description", "id":"first-id","last_modified":1523549895713}]}""")

        val kintoExperiment = Experiment("first-id",
            "first-name",
            "first-description",
            Experiment.Matcher("eng", "first-appId", listOf()),
            Experiment.Bucket(100, 0),
            1523549895713
        )

        val storageExperiment = Experiment("first-id",
            "name",
            "description",
            Experiment.Matcher("es", "appId", listOf("UK")),
            Experiment.Bucket(200, 20),
            1523549800000
        )

        val experimentSource = KintoExperimentSource(baseUrl, bucketName, collectionName, httpClient)
        val kintoExperiments = experimentSource.getExperiments(ExperimentsSnapshot(listOf(storageExperiment), 1523549800000))
        assertEquals(1, kintoExperiments.experiments.size)
        assertEquals(kintoExperiment, kintoExperiments.experiments[0])
        assertEquals(1523549895713, kintoExperiments.lastModified)
    }

    @Test
    fun testGetExperimentsEmptyDiff() {
        val httpClient = mock(HttpClient::class.java)
        `when`(httpClient.get(URL("$baseUrl/buckets/$bucketName/collections/$collectionName/records?_since=1523549895713")))
            .thenReturn("""{"data":[]}""")

        val storageExperiment = Experiment("first-id",
            "first-name",
            "first-description",
            Experiment.Matcher("eng", "first-appId", listOf()),
            Experiment.Bucket(100, 0),
            1523549895713
        )

        val experimentSource = KintoExperimentSource(baseUrl, bucketName, collectionName, httpClient)
        val kintoExperiments = experimentSource.getExperiments(ExperimentsSnapshot(listOf(storageExperiment), 1523549895713))
        assertEquals(1, kintoExperiments.experiments.size)
        assertEquals(storageExperiment, kintoExperiments.experiments[0])
        assertEquals(1523549895713, kintoExperiments.lastModified)
    }
}