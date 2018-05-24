/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard.source.kinto

import mozilla.components.service.fretboard.Experiment
import org.junit.Assert.assertEquals
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
        assertEquals(0, experimentSource.getExperiments(listOf()).size)
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
        val kintoExperiments = experimentSource.getExperiments(listOf())
        assertEquals(1, kintoExperiments.size)
        assertEquals(expectedExperiment, kintoExperiments[0])
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
        val kintoExperiments = experimentSource.getExperiments(listOf(storageExperiment))
        assertEquals(2, kintoExperiments.size)
        assertEquals(storageExperiment, kintoExperiments[0])
        assertEquals(kintoExperiment, kintoExperiments[1])
    }

    @Test
    fun testGetExperimentsDiffDelete() {
        val httpClient = mock(HttpClient::class.java)
        `when`(httpClient.get(URL("$baseUrl/buckets/$bucketName/collections/$collectionName/records?_since=1523549890000")))
            .thenReturn("""{"data":{"deleted":true,"id":"id","last_modified":1523549899999}}""")

        val storageExperiment = Experiment("id",
            "name",
            "description",
            Experiment.Matcher("eng", "appId", listOf("US")),
            Experiment.Bucket(10, 5),
            1523549890000
        )

        val experimentSource = KintoExperimentSource(baseUrl, bucketName, collectionName, httpClient)
        val kintoExperiments = experimentSource.getExperiments(listOf(storageExperiment))
        assertEquals(0, kintoExperiments.size)
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
        val kintoExperiments = experimentSource.getExperiments(listOf(storageExperiment))
        assertEquals(1, kintoExperiments.size)
        assertEquals(kintoExperiment, kintoExperiments[0])
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
        val kintoExperiments = experimentSource.getExperiments(listOf(storageExperiment))
        assertEquals(1, kintoExperiments.size)
        assertEquals(storageExperiment, kintoExperiments[0])
    }
}