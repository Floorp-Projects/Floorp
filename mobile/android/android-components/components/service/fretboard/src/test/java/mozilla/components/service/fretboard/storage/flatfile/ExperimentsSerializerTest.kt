/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard.storage.flatfile

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.service.fretboard.Experiment
import mozilla.components.service.fretboard.ExperimentsSnapshot
import org.json.JSONException
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith

const val experimentsJson = """
                              {
                                  "experiments": [
                                      {
                                          "buckets":{
                                              "min":0,
                                              "max":100
                                          },
                                          "name":"first",
                                          "match":{
                                              "regions": [
                                                  "esp"
                                              ],
                                              "appId":"^org.mozilla.firefox_beta${'$'}",
                                              "lang":"eng|es|deu|fra"
                                          },
                                          "description":"Description",
                                          "id":"experiment-id",
                                          "last_modified":1523549895713
                                      },
                                      {
                                          "buckets":{
                                              "min":5,
                                              "max":10
                                          },
                                          "name":"second",
                                          "match":{
                                              "regions": [
                                                  "deu"
                                              ],
                                              "appId":"^org.mozilla.firefox${'$'}",
                                              "lang":"es|deu"
                                          },
                                          "description":"SecondDescription",
                                          "id":"experiment-2-id",
                                          "last_modified":1523549895749
                                      }
                                  ],
                                  "last_modified": 1523549895749
                              }
                              """

@RunWith(AndroidJUnit4::class)
class ExperimentsSerializerTest {

    @Test
    fun fromJsonValid() {
        val experimentsResult = ExperimentsSerializer().fromJson(experimentsJson)
        val experiments = experimentsResult.experiments
        assertEquals(2, experiments.size)
        assertEquals("first", experiments[0].name)
        assertEquals("eng|es|deu|fra", experiments[0].match!!.language)
        assertEquals("^org.mozilla.firefox_beta${'$'}", experiments[0].match!!.appId)
        assertEquals(1, experiments[0].match!!.regions!!.size)
        assertEquals("esp", experiments[0].match!!.regions!!.get(0))
        assertEquals(100, experiments[0].bucket!!.max)
        assertEquals(0, experiments[0].bucket!!.min)
        assertEquals("Description", experiments[0].description)
        assertEquals("experiment-id", experiments[0].id)
        assertEquals(1523549895713, experiments[0].lastModified)
        assertEquals("second", experiments[1].name)
        assertEquals("es|deu", experiments[1].match!!.language)
        assertEquals("^org.mozilla.firefox${'$'}", experiments[1].match!!.appId)
        assertEquals(1, experiments[1].match!!.regions!!.size)
        assertEquals("deu", experiments[1].match!!.regions!!.get(0))
        assertEquals(10, experiments[1].bucket!!.max)
        assertEquals(5, experiments[1].bucket!!.min)
        assertEquals("SecondDescription", experiments[1].description)
        assertEquals("experiment-2-id", experiments[1].id)
        assertEquals(1523549895749, experiments[1].lastModified)
        assertEquals(1523549895749, experimentsResult.lastModified)
    }

    @Test(expected = JSONException::class)
    fun fromJsonEmptyString() {
        ExperimentsSerializer().fromJson("")
    }

    @Test
    fun fromJsonEmptyArray() {
        val experimentsJson = """ {"experiments":[]} """
        val experimentsResult = ExperimentsSerializer().fromJson(experimentsJson)
        assertEquals(0, experimentsResult.experiments.size)
        assertNull(experimentsResult.lastModified)
    }

    @Test
    fun toJsonValid() {
        val experiments = listOf(
            Experiment("experiment-id",
                "first",
                "Description",
                Experiment.Matcher("eng|es|deu|fra", "^org.mozilla.firefox_beta${'$'}", listOf("esp")),
                Experiment.Bucket(100, 0),
                1523549895713),
            Experiment("experiment-2-id",
                "second",
                "SecondDescription",
                Experiment.Matcher("es|deu", "^org.mozilla.firefox${'$'}", listOf("deu")),
                Experiment.Bucket(10, 5),
                1523549895749)
        )
        val json = JSONObject(ExperimentsSerializer().toJson(ExperimentsSnapshot(experiments, 1523549895749)))
        assertEquals(2, json.length())
        assertEquals(1523549895749, json.getLong("last_modified"))
        val experimentsArray = json.getJSONArray("experiments")
        assertEquals(2, experimentsArray.length())
        val firstExperiment = experimentsArray[0] as JSONObject
        val firstExperimentBuckets = firstExperiment.getJSONObject("buckets")
        assertEquals(0, firstExperimentBuckets.getInt("min"))
        assertEquals(100, firstExperimentBuckets.getInt("max"))
        assertEquals("first", firstExperiment.getString("name"))
        val firstExperimentMatch = firstExperiment.getJSONObject("match")
        val firstExperimentRegions = firstExperimentMatch.getJSONArray("regions")
        assertEquals(1, firstExperimentRegions.length())
        assertEquals("esp", firstExperimentRegions[0])
        assertEquals("^org.mozilla.firefox_beta${'$'}", firstExperimentMatch.getString("appId"))
        assertEquals("eng|es|deu|fra", firstExperimentMatch.getString("lang"))
        assertEquals("Description", firstExperiment.getString("description"))
        assertEquals("experiment-id", firstExperiment.getString("id"))
        assertEquals(1523549895713, firstExperiment.getLong("last_modified"))

        val secondExperiment = experimentsArray[1] as JSONObject
        val secondExperimentBuckets = secondExperiment.getJSONObject("buckets")
        assertEquals(5, secondExperimentBuckets.getInt("min"))
        assertEquals(10, secondExperimentBuckets.getInt("max"))
        assertEquals("second", secondExperiment.getString("name"))
        val secondExperimentMatch = secondExperiment.getJSONObject("match")
        val secondExperimentRegions = secondExperimentMatch.getJSONArray("regions")
        assertEquals(1, secondExperimentRegions.length())
        assertEquals("deu", secondExperimentRegions[0])
        assertEquals("^org.mozilla.firefox${'$'}", secondExperimentMatch.getString("appId"))
        assertEquals("es|deu", secondExperimentMatch.getString("lang"))
        assertEquals("SecondDescription", secondExperiment.getString("description"))
        assertEquals("experiment-2-id", secondExperiment.getString("id"))
        assertEquals(1523549895749, secondExperiment.getLong("last_modified"))
    }

    @Test
    fun toJsonEmptyList() {
        val experiments = listOf<Experiment>()
        val experimentsJson = JSONObject(ExperimentsSerializer().toJson(ExperimentsSnapshot(experiments, null)))
        assertEquals(1, experimentsJson.length())
        val experimentsJsonArray = experimentsJson.getJSONArray("experiments")
        assertEquals(0, experimentsJsonArray.length())
    }
}