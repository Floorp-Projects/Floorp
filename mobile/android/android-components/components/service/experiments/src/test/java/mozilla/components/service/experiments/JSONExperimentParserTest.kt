/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.ktx.android.org.json.tryGetInt
import org.json.JSONException
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class JSONExperimentParserTest {
    private var currentTime = System.currentTimeMillis() / 1000

    @Test
    fun toJson() {
        val experiment = createDefaultExperiment(
            id = "sample-id",
            description = "sample-description",
            match = createDefaultMatcher(
                appId = "sample-appId",
                regions = listOf("US"),
                appDisplayVersion = "1.0",
                appMinVersion = "0.1.0",
                appMaxVersion = "1.1.0",
                deviceManufacturer = "manufacturer",
                deviceModel = "device",
                localeCountry = "country",
                localeLanguage = "es|en"
            ),
            buckets = Experiment.Buckets(0, 20),
            branches = listOf(
                Experiment.Branch("branch1", 1),
                Experiment.Branch("branch2", 2),
                Experiment.Branch("branch3", 1)
            ),
            lastModified = currentTime
        )

        val jsonObject = JSONExperimentParser().toJson(experiment)

        assertEquals("sample-id", jsonObject.getString("id"))
        assertEquals("sample-description", jsonObject.getString("description"))
        assertEquals(currentTime, jsonObject.getLong("last_modified"))

        val buckets = jsonObject.getJSONObject("buckets")
        assertEquals(0, buckets.getInt("start"))
        assertEquals(20, buckets.getInt("count"))

        val branches = jsonObject.getJSONArray("branches")
        assertEquals("branch1", branches.getJSONObject(0).getString("name"))
        assertEquals(1, branches.getJSONObject(0).getInt("ratio"))
        assertEquals("branch2", branches.getJSONObject(1).getString("name"))
        assertEquals(2, branches.getJSONObject(1).getInt("ratio"))
        assertEquals("branch3", branches.getJSONObject(2).getString("name"))
        assertEquals(1, branches.getJSONObject(2).getInt("ratio"))

        val match = jsonObject.getJSONObject("match")
        val regions = match.getJSONArray("regions")
        assertEquals(1, regions.length())
        assertEquals("US", regions.get(0))
        assertEquals("sample-appId", match.getString("app_id"))
        assertEquals("es|en", match.getString("locale_language"))
        assertEquals("1.0", match.getString("app_display_version"))
        assertEquals("0.1.0", match.getString("app_min_version"))
        assertEquals("1.1.0", match.getString("app_max_version"))
        assertEquals("manufacturer", match.getString("device_manufacturer"))
        assertEquals("device", match.getString("device_model"))
        assertEquals("country", match.getString("locale_country"))
    }

    @Test
    fun `toJson for null values`() {
        val experiment = Experiment(
            id = "",
            description = "",
            lastModified = null,
            schemaModified = null,
            buckets = Experiment.Buckets(0, 0),
            branches = emptyList(),
            match = Experiment.Matcher(
                null,
                null,
                null,
                null,
                null,
                null,
                null,
                null,
                null,
                null
            )
        )

        val jsonObject = JSONExperimentParser().toJson(experiment)
        assertEquals("", jsonObject.getString("id"))
        assertEquals("", jsonObject.getString("description"))
        assertNull(jsonObject.tryGetInt("last_modified"))
        assertNull(jsonObject.tryGetInt("schema"))

        val buckets = jsonObject.getJSONObject("buckets")
        assertEquals(0, buckets.getInt("start"))
        assertEquals(0, buckets.getInt("count"))

        val branches = jsonObject.getJSONArray("branches")
        assertEquals(0, branches.length())

        val match = jsonObject.getJSONObject("match")
        assertFalse(match.keys().hasNext())
    }

    @Test
    fun fromJson() {
        val json = """
            {
              "id": "sample-id",
              "description": "sample-description",
              "buckets": {
                "start": 0,
                "count": 20
              },
              "branches": [
                {
                  "name": "branch0",
                  "ratio": "1"
                }
              ],
              "match": {
                "regions": [
                  "US"
                ],
                "app_id": "sample-appId",
                "locale_language": "es|en",
                "app_min_version": "1.0.0",
                "app_max_version": "1.1.0"
              },
              "last_modified": $currentTime
            }
        """.trimIndent()

        val expectedExperiment = createDefaultExperiment(
            id = "sample-id",
            description = "sample-description",
            match = createDefaultMatcher(
                localeLanguage = "es|en",
                appId = "sample-appId",
                regions = listOf("US"),
                appMinVersion = "1.0.0",
                appMaxVersion = "1.1.0"
            ),
            buckets = Experiment.Buckets(0, 20),
            lastModified = currentTime
        )
        assertEquals(expectedExperiment, JSONExperimentParser().fromJson(JSONObject(json)))
    }

    @Test
    fun `fromJson() with debug tag`() {
        val json = """
            {
              "id": "sample-id",
              "description": "sample-description",
              "buckets": {
                "start": 0,
                "count": 20
              },
              "branches": [
                {
                  "name": "branch0",
                  "ratio": "1"
                }
              ],
              "match": {
                "regions": [
                  "US"
                ],
                "app_id": "sample-appId",
                "locale_language": "es|en"
              },
              "last_modified": $currentTime,
              "debug_tags": ["dummy-tag"]
            }
        """.trimIndent()

        val expectedExperiment = createDefaultExperiment(
            id = "sample-id",
            description = "sample-description",
            match = createDefaultMatcher(
                localeLanguage = "es|en",
                appId = "sample-appId",
                regions = listOf("US"),
                debugTags = listOf("dummy-tags")
            ),
            buckets = Experiment.Buckets(0, 20),
            lastModified = currentTime
        )
        assertEquals(expectedExperiment, JSONExperimentParser().fromJson(JSONObject(json)))
    }

    @Test(expected = JSONException::class)
    fun `fromJson with non present values`() {
        val json = """
            {
                "id": "id",
            }
        """.trimIndent()
        JSONExperimentParser().fromJson(JSONObject(json))
    }

    @Test
    fun `fromJson for null values`() {
        val json = """
            {
              "id": null,
              "match": {
              },
              "buckets": {
                "start": 0,
                "count": 0
              },
              "branches": [
                {
                  "name": "branch0",
                  "ratio": "1"
                }
              ],
              "description": null,
              "last_modified": null
            }
        """.trimIndent()
        var experiment = createDefaultExperiment(id = "sample-id")
        assertNotEquals(experiment, JSONExperimentParser().fromJson(JSONObject(json)))

        val emptyObjects = """
            {
              "id": "sample-id",
              "description": "",
              "buckets": {
                "start": 0,
                "count": 0
              },
              "branches": [
                {
                  "name": "branch0",
                  "ratio": "1"
                }
              ],
              "match": {
                "locale_language": null,
                "app_id": null,
                "regions": null
              }
            }
        """.trimIndent()
        experiment = createDefaultExperiment(id = "sample-id")
        assertEquals(experiment, JSONExperimentParser().fromJson(JSONObject(emptyObjects)))
    }

    @Test(expected = JSONException::class)
    fun `fromJson with empty branches list`() {
        val json = """
            {
              "id": "some-id",
              "match": {
              },
              "buckets": {
                "start": 0,
                "count": 0
              },
              "branches": [
              ],
              "description": "",
              "last_modified": 1234
            }
        """.trimIndent()
        JSONExperimentParser().fromJson(JSONObject(json))
    }
}
