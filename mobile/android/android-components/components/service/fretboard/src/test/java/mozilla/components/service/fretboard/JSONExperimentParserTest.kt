/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class JSONExperimentParserTest {

    @Test
    fun toJson() {
        val experiment = Experiment("sample-id",
            "sample-name",
            "sample-description",
            Experiment.Matcher("es|en",
                "sample-appId",
                listOf("US"),
                "1.0",
                "manufacturer",
                "device",
                "country",
                "release_channel"),
            Experiment.Bucket(20, 0),
            1526991669)
        val jsonObject = JSONExperimentParser().toJson(experiment)
        val buckets = jsonObject.getJSONObject("buckets")
        assertEquals(0, buckets.getInt("min"))
        assertEquals(20, buckets.getInt("max"))
        assertEquals("sample-name", jsonObject.getString("name"))
        val match = jsonObject.getJSONObject("match")
        val regions = match.getJSONArray("regions")
        assertEquals(1, regions.length())
        assertEquals("US", regions.get(0))
        assertEquals("sample-appId", match.getString("appId"))
        assertEquals("es|en", match.getString("lang"))
        assertEquals("1.0", match.getString("version"))
        assertEquals("manufacturer", match.getString("manufacturer"))
        assertEquals("device", match.getString("device"))
        assertEquals("country", match.getString("country"))
        assertEquals("release_channel", match.getString("release_channel"))
        assertEquals("sample-description", jsonObject.getString("description"))
        assertEquals("sample-id", jsonObject.getString("id"))
        assertEquals(1526991669, jsonObject.getLong("last_modified"))
    }

    @Test
    fun toJsonNullValues() {
        val experiment = Experiment("id", "name")
        val jsonObject = JSONExperimentParser().toJson(experiment)
        val buckets = jsonObject.getJSONObject("buckets")
        assertEquals(0, buckets.length())
        val match = jsonObject.getJSONObject("match")
        assertEquals(0, match.length())
        assertEquals("id", jsonObject.getString("id"))
        assertEquals("name", jsonObject.getString("name"))
    }

    @Test
    fun fromJson() {
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
    fun fromJsonNonPresentValues() {
        val json = """{"id":"id","name":"name"}"""
        assertEquals(Experiment("id", "name"), JSONExperimentParser().fromJson(JSONObject(json)))
    }

    @Test
    fun fromJsonNullValues() {
        val json = """{"buckets":null,"name":"sample-name","match":null,"description":null,"id":"sample-id","last_modified":null}"""
        assertEquals(Experiment("sample-id", "sample-name"), JSONExperimentParser().fromJson(JSONObject(json)))
        val emptyObjects = """{"id":"sample-id","name":"sample-name","buckets":{"min":null,"max":null},"match":{"lang":null,"appId":null,"region":null}}"""
        assertEquals(Experiment("sample-id", name = "sample-name", bucket = Experiment.Bucket(), match = Experiment.Matcher()),
            JSONExperimentParser().fromJson(JSONObject(emptyObjects)))
    }

    @Test
    fun payloadFromJson() {
        val json = """{"buckets":null,"name":null,"match":null,"description":null,"id":"sample-id","last_modified":null,"values":{"a":"a","b":3,"c":3.5,"d":true,"e":[1,2,3,4]}}"""
        val experiment = JSONExperimentParser().fromJson(JSONObject(json))
        assertEquals("a", experiment.payload?.get("a"))
        assertEquals(3, experiment.payload?.get("b"))
        assertEquals(3.5, experiment.payload?.get("c"))
        assertEquals(true, experiment.payload?.get("d"))
        assertEquals(listOf(1, 2, 3, 4), experiment.payload?.getIntList("e"))
    }

    @Test
    fun payloadToJson() {
        val payload = ExperimentPayload()
        payload.put("a", "a")
        payload.put("b", 3)
        payload.put("c", 3.5)
        payload.put("d", true)
        payload.put("e", listOf(1, 2, 3, 4))
        val experiment = Experiment("id", name = "name", payload = payload)
        val json = JSONExperimentParser().toJson(experiment)
        val payloadJson = json.getJSONObject("values")
        assertEquals("a", payloadJson.getString("a"))
        assertEquals(3, payloadJson.getInt("b"))
        assertEquals(3.5, payloadJson.getDouble("c"), 0.01)
        assertEquals(true, payloadJson.getBoolean("d"))
        val list = payloadJson.getJSONArray("e")
        assertEquals(4, list.length())
        assertEquals(1, list[0])
        assertEquals(2, list[1])
        assertEquals(3, list[2])
        assertEquals(4, list[3])
        val buckets = json.getJSONObject("buckets")
        assertEquals(0, buckets.length())
        val match = json.getJSONObject("match")
        assertEquals(0, match.length())
        assertEquals("id", json.getString("id"))
    }
}
