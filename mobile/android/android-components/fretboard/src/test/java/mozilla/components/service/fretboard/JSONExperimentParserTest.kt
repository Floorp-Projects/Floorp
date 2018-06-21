/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class JSONExperimentParserTest {
    @Test
    fun testToJson() {
        val experiment = Experiment("sample-id",
            "sample-name",
            "sample-description",
            Experiment.Matcher("es|en", "sample-appId", listOf("US")),
            Experiment.Bucket(20, 0),
            1526991669)
        assertEquals("""{"buckets":{"min":0,"max":20},"name":"sample-name","match":{"regions":["US"],"appId":"sample-appId","lang":"es|en"},"description":"sample-description","id":"sample-id","last_modified":1526991669}""",
            JSONExperimentParser().toJson(experiment).toString())
    }

    @Test
    fun testToJsonNullValues() {
        val experiment = Experiment("id")
        assertEquals("""{"buckets":{},"match":{},"id":"id"}""", JSONExperimentParser().toJson(experiment).toString())
    }

    @Test
    fun testFromJson() {
        val json = """{"buckets":{"min":0,"max":20},"name":"sample-name","match":{"regions":["US"],"appId":"sample-appId","lang":"es|en"},"description":"sample-description","id":"sample-id","last_modified":1526991669}"""
        val expectedExperiment = Experiment("sample-id",
            "sample-name",
            "sample-description",
            Experiment.Matcher("es|en", "sample-appId", listOf("US")),
            Experiment.Bucket(20, 0),
            1526991669)
        assertEquals(expectedExperiment, JSONExperimentParser().fromJson(JSONObject(json)))
    }

    @Test
    fun testFromJsonNonPresentValues() {
        val json = """{"id":"id"}"""
        assertEquals(Experiment("id"), JSONExperimentParser().fromJson(JSONObject(json)))
    }

    @Test
    fun testFromJsonNullValues() {
        val json = """{"buckets":null,"name":null,"match":null,"description":null,"id":"sample-id","last_modified":null}"""
        assertEquals(Experiment("sample-id"), JSONExperimentParser().fromJson(JSONObject(json)))
        val emptyObjects = """{"id":"sample-id","buckets":{"min":null,"max":null},"match":{"lang":null,"appId":null,"region":null}}"""
        assertEquals(Experiment("sample-id", bucket = Experiment.Bucket(), match = Experiment.Matcher()),
            JSONExperimentParser().fromJson(JSONObject(emptyObjects)))
    }

    @Test
    fun testPayloadFromJson() {
        val json = """{"buckets":null,"name":null,"match":null,"description":null,"id":"sample-id","last_modified":null,"payload":{"a":"a","b":3,"c":3.5,"d":true,"e":[1,2,3,4]}}"""
        val experiment = JSONExperimentParser().fromJson(JSONObject(json))
        assertEquals("a", experiment.payload?.get("a"))
        assertEquals(3, experiment.payload?.get("b"))
        assertEquals(3.5, experiment.payload?.get("c"))
        assertEquals(true, experiment.payload?.get("d"))
        assertEquals(listOf(1, 2, 3, 4), experiment.payload?.getIntList("e"))
    }

    @Test
    fun testPayloadToJson() {
        val payload = ExperimentPayload()
        payload.put("a", "a")
        payload.put("b", 3)
        payload.put("c", 3.5)
        payload.put("d", true)
        payload.put("e", listOf(1, 2, 3, 4))
        val experiment = Experiment("id", payload = payload)
        val json = JSONExperimentParser().toJson(experiment)
        assertEquals("""{"payload":{"a":"a","b":3,"c":3.5,"d":true,"e":[1,2,3,4]},"buckets":{},"match":{},"id":"id"}""", json.toString())
    }
}