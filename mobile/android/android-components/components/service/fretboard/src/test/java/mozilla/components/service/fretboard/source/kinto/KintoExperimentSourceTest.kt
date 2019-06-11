/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard.source.kinto

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Response
import mozilla.components.service.fretboard.Experiment
import mozilla.components.service.fretboard.ExperimentsSnapshot
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`

@RunWith(AndroidJUnit4::class)
class KintoExperimentSourceTest {

    private val baseUrl = "http://mydomain.test"
    private val bucketName = "fretboard"
    private val collectionName = "experiments"

    @Test
    fun noExperiments() {
        val httpClient = mock<Client>()

        val url = "$baseUrl/buckets/$bucketName/collections/$collectionName/records"
        `when`(httpClient.fetch(any()))
            .thenReturn(Response(url, 200, MutableHeaders(), Response.Body("""{"data":[]}""".byteInputStream())))
        val experimentSource = KintoExperimentSource(baseUrl, bucketName, collectionName, httpClient)
        val result = experimentSource.getExperiments(ExperimentsSnapshot(listOf(), null))
        assertEquals(0, result.experiments.size)
        assertNull(result.lastModified)
    }

    @Test
    fun getExperimentsNoDiff() {
        val httpClient = mock<Client>()

        val url = "$baseUrl/buckets/$bucketName/collections/$collectionName/records"
        `when`(httpClient.fetch(any())).thenReturn(
            Response(url,
                200,
                MutableHeaders(),
                Response.Body("""{"data":[{"name":"first-name","match":{"lang":"eng","appId":"first-appId",regions:[]},"schema":1523549592861,"buckets":{"max":"100","min":"0"},"description":"first-description", "id":"first-id","last_modified":1523549895713}]}""".byteInputStream())))

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
    fun getExperimentsDiffAdd() {
        val httpClient = mock<Client>()
        val url = "$baseUrl/buckets/$bucketName/collections/$collectionName/records?_since=1523549890000"
        `when`(httpClient.fetch(any())).thenReturn(
            Response(url,
                200,
                MutableHeaders(),
                Response.Body("""{"data":[{"name":"first-name","match":{"lang":"eng","appId":"first-appId",regions:[]},"schema":1523549592861,"buckets":{"max":"100","min":"0"},"description":"first-description", "id":"first-id","last_modified":1523549895713}]}""".byteInputStream())))

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
    fun getExperimentsDiffDelete() {
        val httpClient = mock<Client>()

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

        `when`(httpClient.fetch(any())).thenReturn(
                Response("$baseUrl/buckets/$bucketName/collections/$collectionName/records?_since=1523549890000",
                        200,
                        MutableHeaders(),
                        Response.Body("""{"data":[{"deleted":true,"id":"id","last_modified":1523549899999}]}""".byteInputStream())))

        val experimentSource = KintoExperimentSource(baseUrl, bucketName, collectionName, httpClient)
        val kintoExperiments = experimentSource.getExperiments(ExperimentsSnapshot(listOf(storageExperiment, secondExperiment), 1523549890000))
        assertEquals(1, kintoExperiments.experiments.size)
        assertEquals(1523549899999, kintoExperiments.lastModified)

        `when`(httpClient.fetch(any())).thenReturn(
                Response("$baseUrl/buckets/$bucketName/collections/$collectionName/records?_since=1523549899999",
                        200,
                        MutableHeaders(),
                        Response.Body("""{"data":[]}""".byteInputStream())))

        val experimentsResult = experimentSource.getExperiments(ExperimentsSnapshot(kintoExperiments.experiments, 1523549899999))
        assertEquals(1, experimentsResult.experiments.size)
        assertEquals(1523549899999, experimentsResult.lastModified)
    }

    @Test
    fun getExperimentsDiffUpdate() {
        val httpClient = mock<Client>()
        val url = "$baseUrl/buckets/$bucketName/collections/$collectionName/records?_since=1523549800000"
        `when`(httpClient.fetch(any())).thenReturn(
            Response(url,
                200,
                MutableHeaders(),
                Response.Body("""{"data":[{"name":"first-name","match":{"lang":"eng","appId":"first-appId",regions:[]},"schema":1523549592861,"buckets":{"max":"100","min":"0"},"description":"first-description", "id":"first-id","last_modified":1523549895713}]}""".byteInputStream())))

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
    fun getExperimentsEmptyDiff() {
        val httpClient = mock<Client>()
        val url = "$baseUrl/buckets/$bucketName/collections/$collectionName/records?_since=1523549895713"
        `when`(httpClient.fetch(any())).thenReturn(
            Response(url,
                200,
                MutableHeaders(),
                Response.Body("""{"data":[]}""".byteInputStream())))

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