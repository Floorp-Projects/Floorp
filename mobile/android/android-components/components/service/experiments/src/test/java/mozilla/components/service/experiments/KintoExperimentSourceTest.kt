/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Response
import mozilla.components.support.test.any
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock

@RunWith(AndroidJUnit4::class)
class KintoExperimentSourceTest {
    private val baseUrl = "http://mydomain.test"
    private val bucketName = "fretboard"
    private val collectionName = "experiments"
    private var currentTime = System.currentTimeMillis() / 1000
    private var pastTime = currentTime - 1000

    @Test
    fun noExperiments() {
        val httpClient = mock(Client::class.java)
        val url = "$baseUrl/buckets/$bucketName/collections/$collectionName/records"
        val json = """
            {
                "data": []
            }
        """.trimIndent()

        `when`(httpClient.fetch(any()))
            .thenReturn(Response(url, 200, MutableHeaders(), Response.Body(json.byteInputStream())))
        val experimentSource = KintoExperimentSource(baseUrl, bucketName, collectionName, httpClient)
        val result = experimentSource.getExperiments(ExperimentsSnapshot(listOf(), null))
        assertEquals(0, result.experiments.size)
        assertNull(result.lastModified)
    }

    @Test
    fun getExperimentsNoDiff() {
        val httpClient = mock(Client::class.java)

        val url = "$baseUrl/buckets/$bucketName/collections/$collectionName/records"
        val json = """
            {
                "data":[
                    {
                        "id": "first-id",
                        "id": "first-id",
                        "description": "first-description",
                        "last_modified": $pastTime,
                        "match": {
                            "locale_language": "eng",
                            "app_id": "first-appId",
                            "regions": []
                        },
                        "schema": 1523549592861,
                        "buckets": {
                            "start": "0",
                            "count": "100"
                        },
                        "branches": [
                            {
                                "name": "branch1",
                                "ratio": "1"
                            }
                        ]
                    }
                ]
            }
        """.trimIndent()

        `when`(httpClient.fetch(any())).thenReturn(
            Response(url,
                200,
                MutableHeaders(),
                Response.Body(json.byteInputStream())))

        val expectedExperiment = createDefaultExperiment(
            id = "first-id",
            description = "first-description",
            match = createDefaultMatcher(
                localeLanguage = "eng",
                appId = "first-appId",
                regions = listOf()
            ),
            buckets = Experiment.Buckets(0, 100),
            lastModified = pastTime
        )

        val experimentSource = KintoExperimentSource(baseUrl, bucketName, collectionName, httpClient)
        val kintoExperiments = experimentSource.getExperiments(ExperimentsSnapshot(listOf(), null))
        assertEquals(1, kintoExperiments.experiments.size)
        assertEquals(expectedExperiment, kintoExperiments.experiments[0])
        assertEquals(pastTime, kintoExperiments.lastModified)
    }

    @Test
    fun getExperimentsDiffAdd() {
        val httpClient = mock(Client::class.java)
        val url = "$baseUrl/buckets/$bucketName/collections/$collectionName/records?_since=1523549890000"
        val json = """
            {
                "data": [
                    {
                        "id": "first-id",
                        "description": "first-description",
                        "last_modified": $pastTime,
                        "match": {
                            "locale_language": "eng",
                            "app_id": "first-appId",
                            "regions": []
                        },
                        "schema": 1523549592861,
                        "buckets": {
                            "start": "0",
                            "count": "100"
                        },
                        "branches": [
                            {
                                "name": "branch1",
                                "ratio": "1"
                            }
                        ]
                    }
                ]
            }
        """.trimIndent()

        `when`(httpClient.fetch(any())).thenReturn(
            Response(url,
                200,
                MutableHeaders(),
                Response.Body(json.byteInputStream())))

        val kintoExperiment = createDefaultExperiment(
            id = "first-id",
            description = "first-description",
            match = createDefaultMatcher(
                localeLanguage = "eng",
                appId = "first-appId",
                regions = listOf()
            ),
            buckets = Experiment.Buckets(0, 100),
            lastModified = currentTime
        )

        val storageExperiment = createDefaultExperiment(
            id = "id",
            description = "description",
            match = createDefaultMatcher(
                localeLanguage = "eng",
                appId = "appId",
                regions = listOf("US")
            ),
            buckets = Experiment.Buckets(5, 5),
            lastModified = pastTime
        )

        val experimentSource = KintoExperimentSource(baseUrl, bucketName, collectionName, httpClient)
        val kintoExperiments = experimentSource.getExperiments(ExperimentsSnapshot(listOf(storageExperiment), pastTime))
        assertEquals(2, kintoExperiments.experiments.size)
        assertEquals(storageExperiment, kintoExperiments.experiments[0])
        assertEquals(kintoExperiment, kintoExperiments.experiments[1])
        assertEquals(pastTime, kintoExperiments.lastModified)
    }

    @Test
    fun getExperimentsDiffDelete() {
        val httpClient = mock(Client::class.java)

        val storageExperiment = createDefaultExperiment(
            id = "id",
            description = "description",
            match = createDefaultMatcher(
                localeLanguage = "eng",
                appId = "appId",
                regions = listOf("US")
            ),
            buckets = Experiment.Buckets(5, 5),
            lastModified = pastTime
        )

        val secondExperiment = createDefaultExperiment(
            id = "id2",
            description = "description2",
            match = createDefaultMatcher(
                localeLanguage = "eng",
                appId = "appId",
                regions = listOf("US")
            ),
            buckets = Experiment.Buckets(5, 5),
            lastModified = pastTime
        )

        val json = """
            {
                "data": [
                    {
                        "deleted": true,
                        "id": "id",
                        "last_modified": $currentTime
                    }
                ]
            }
        """.trimIndent()

        `when`(httpClient.fetch(any())).thenReturn(
                Response("$baseUrl/buckets/$bucketName/collections/$collectionName/records?_since=$pastTime",
                        200,
                        MutableHeaders(),
                        Response.Body(json.byteInputStream())))

        val experimentSource = KintoExperimentSource(baseUrl, bucketName, collectionName, httpClient)
        val kintoExperiments = experimentSource.getExperiments(ExperimentsSnapshot(listOf(storageExperiment, secondExperiment), pastTime))
        assertEquals(1, kintoExperiments.experiments.size)
        assertEquals(currentTime, kintoExperiments.lastModified)

        val json2 = """
            {
                "data": []
            }
        """.trimIndent()

        `when`(httpClient.fetch(any())).thenReturn(
                Response("$baseUrl/buckets/$bucketName/collections/$collectionName/records?_since=1523549899999",
                        200,
                        MutableHeaders(),
                        Response.Body(json2.byteInputStream())))

        val experimentsResult = experimentSource.getExperiments(ExperimentsSnapshot(kintoExperiments.experiments, currentTime))
        assertEquals(1, experimentsResult.experiments.size)
        assertEquals(currentTime, experimentsResult.lastModified)
    }

    @Test
    fun getExperimentsDiffUpdate() {
        val httpClient = mock(Client::class.java)
        val url = "$baseUrl/buckets/$bucketName/collections/$collectionName/records?_since=1523549800000"

        val json = """
            {
                "data": [
                    {
                        "id": "first-id",
                        "description": "first-description",
                        "last_modified": $currentTime,
                        "match": {
                            "locale_language": "eng",
                            "app_id": "first-appId",
                            "regions": []
                        },
                        "schema": 1523549592861,
                        "buckets": {
                            "start": "0",
                            "count": "100"
                        },
                        "branches": [
                            {
                                "name": "branch1",
                                "ratio": "1"
                            }
                        ]
                    }
                ]
            }
        """.trimIndent()

        `when`(httpClient.fetch(any())).thenReturn(
            Response(url,
                200,
                MutableHeaders(),
                Response.Body(json.byteInputStream())))

        val kintoExperiment = createDefaultExperiment(
            id = "first-id",
            description = "first-description",
            match = createDefaultMatcher(
                localeLanguage = "eng",
                appId = "first-appId",
                regions = listOf()
            ),
            buckets = Experiment.Buckets(0, 100),
            lastModified = currentTime
        )

        val storageExperiment = createDefaultExperiment(
            id = "first-id",
            description = "description",
            match = createDefaultMatcher(
                localeLanguage = "es",
                appId = "appId",
                regions = listOf("UK")
            ),
            buckets = Experiment.Buckets(20, 180),
            lastModified = pastTime
        )

        val experimentSource = KintoExperimentSource(baseUrl, bucketName, collectionName, httpClient)
        val kintoExperiments = experimentSource.getExperiments(ExperimentsSnapshot(listOf(storageExperiment), pastTime))
        assertEquals(1, kintoExperiments.experiments.size)
        assertEquals(kintoExperiment, kintoExperiments.experiments[0])
        assertEquals(currentTime, kintoExperiments.lastModified)
    }

    @Test
    fun getExperimentsEmptyDiff() {
        val httpClient = mock(Client::class.java)
        val url = "$baseUrl/buckets/$bucketName/collections/$collectionName/records?_since=$pastTime"

        val json = """
            {
                "data": []
            }
        """.trimIndent()

        `when`(httpClient.fetch(any())).thenReturn(
            Response(url,
                200,
                MutableHeaders(),
                Response.Body(json.byteInputStream())))

        val storageExperiment = createDefaultExperiment(
            id = "first-id",
            description = "first-description",
            match = createDefaultMatcher(
                localeLanguage = "eng",
                appId = "first-appId",
                regions = listOf()
            ),
            buckets = Experiment.Buckets(0, 100),
            lastModified = pastTime
        )

        val experimentSource = KintoExperimentSource(baseUrl, bucketName, collectionName, httpClient)
        val kintoExperiments = experimentSource.getExperiments(ExperimentsSnapshot(listOf(storageExperiment), pastTime))
        assertEquals(1, kintoExperiments.experiments.size)
        assertEquals(storageExperiment, kintoExperiments.experiments[0])
        assertEquals(pastTime, kintoExperiments.lastModified)
    }
}