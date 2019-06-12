/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.ktx.android.org.json.toList
import org.json.JSONException
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class ExperimentsSerializerTest {
    private var currentTime = System.currentTimeMillis() / 1000
    private var pastTime = currentTime - 1000

    private val experimentsJson = """
{
  "experiments": [
      {
          "id":"first",
          "description":"Description",
          "last_modified":$pastTime,
          "buckets":{
              "start":0,
              "count":100
          },
          "branches":[
              {
                  "name":"first-branch",
                  "ratio":7
              },
              {
                  "name":"second-branch",
                  "ratio":21
              }
          ],
          "match":{
              "regions": [
                  "esp"
              ],
              "app_id":"^org.mozilla.firefox_beta${'$'}",
              "locale_language":"eng|es|deu|fra"
          }
      },
      {
          "id":"second",
          "description":"SecondDescription",
          "last_modified":$currentTime,
          "buckets":{
              "start":5,
              "count":5
          },
          "branches":[
              {
                  "name":"one-branch",
                  "ratio":70
              }
          ],
          "match":{
              "regions": [
                  "deu"
              ],
              "app_id":"^org.mozilla.firefox${'$'}",
              "locale_language":"es|deu"
          }
      }
  ],
  "last_modified": $currentTime
}
"""

    @Test
    fun fromJsonValid() {
        val experimentsResult = ExperimentsSerializer().fromJson(experimentsJson)
        val experiments = experimentsResult.experiments

        assertEquals(2, experiments.size)
        assertEquals(currentTime, experimentsResult.lastModified)

        assertEquals("first", experiments[0].id)
        assertEquals("Description", experiments[0].description)
        assertEquals(pastTime, experiments[0].lastModified)

        assertEquals("eng|es|deu|fra", experiments[0].match.localeLanguage)
        assertEquals("^org.mozilla.firefox_beta${'$'}", experiments[0].match.appId)
        assertEquals(1, experiments[0].match.regions!!.size)
        assertEquals("esp", experiments[0].match.regions!![0])

        assertEquals(0, experiments[0].buckets.start)
        assertEquals(100, experiments[0].buckets.count)

        assertEquals(2, experiments[0].branches.size)
        assertEquals("first-branch", experiments[0].branches[0].name)
        assertEquals(7, experiments[0].branches[0].ratio)
        assertEquals("second-branch", experiments[0].branches[1].name)
        assertEquals(21, experiments[0].branches[1].ratio)

        assertEquals("second", experiments[1].id)
        assertEquals("SecondDescription", experiments[1].description)
        assertEquals(currentTime, experiments[1].lastModified)

        assertEquals("es|deu", experiments[1].match.localeLanguage)
        assertEquals("^org.mozilla.firefox${'$'}", experiments[1].match.appId)
        assertEquals(1, experiments[1].match.regions!!.size)
        assertEquals("deu", experiments[1].match.regions!![0])

        assertEquals(5, experiments[1].buckets.start)
        assertEquals(5, experiments[1].buckets.count)

        assertEquals(1, experiments[1].branches.size)
        assertEquals("one-branch", experiments[1].branches[0].name)
        assertEquals(70, experiments[1].branches[0].ratio)
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
            createDefaultExperiment(
                id = "experiment-1-id",
                description = "Description",
                match = createDefaultMatcher(
                    localeLanguage = "eng|es|deu|fra",
                    appId = "^org.mozilla.firefox_beta${'$'}",
                    regions = listOf("esp")
                ),
                buckets = Experiment.Buckets(0, 100),
                branches = emptyList(),
                lastModified = pastTime
            ),
            createDefaultExperiment(
                id = "experiment-2-id",
                description = "SecondDescription",
                match = createDefaultMatcher(
                    localeLanguage = "es|deu",
                    appId = "^org.mozilla.firefox${'$'}",
                    regions = listOf("deu")
                ),
                branches = listOf(
                    Experiment.Branch("branch1", 5),
                    Experiment.Branch("branch2", 7)
                ),
                buckets = Experiment.Buckets(5, 5),
                lastModified = currentTime
            )
        )

        val json = JSONObject(ExperimentsSerializer().toJson(ExperimentsSnapshot(experiments, currentTime)))
        assertEquals(2, json.length())
        assertEquals(currentTime, json.getLong("last_modified"))
        val experimentsArray = json.getJSONArray("experiments")
        assertEquals(2, experimentsArray.length())

        val firstExperiment = experimentsArray[0] as JSONObject
        assertEquals("experiment-1-id", firstExperiment.getString("id"))
        assertEquals("Description", firstExperiment.getString("description"))
        assertEquals(pastTime, firstExperiment.getLong("last_modified"))

        val firstExperimentBuckets = firstExperiment.getJSONObject("buckets")
        assertEquals(0, firstExperimentBuckets.getInt("start"))
        assertEquals(100, firstExperimentBuckets.getInt("count"))

        val firstExperimentBranches = firstExperiment.getJSONArray("branches")
        assertEquals(0, firstExperimentBranches.length())

        val firstExperimentMatch = firstExperiment.getJSONObject("match")
        val firstExperimentRegions = firstExperimentMatch.getJSONArray("regions")
        assertEquals(listOf("esp"), firstExperimentRegions.toList<String>())
        assertEquals("^org.mozilla.firefox_beta${'$'}", firstExperimentMatch.getString("app_id"))
        assertEquals("eng|es|deu|fra", firstExperimentMatch.getString("locale_language"))

        val secondExperiment = experimentsArray[1] as JSONObject
        assertEquals("experiment-2-id", secondExperiment.getString("id"))
        assertEquals("SecondDescription", secondExperiment.getString("description"))
        assertEquals(currentTime, secondExperiment.getLong("last_modified"))

        val secondExperimentBuckets = secondExperiment.getJSONObject("buckets")
        assertEquals(5, secondExperimentBuckets.getInt("start"))
        assertEquals(5, secondExperimentBuckets.getInt("count"))

        val secondExperimentBranches = secondExperiment.getJSONArray("branches").toList<JSONObject>()
        assertEquals("branch1", secondExperimentBranches[0].getString("name"))
        assertEquals(5, secondExperimentBranches[0].getInt("ratio"))
        assertEquals("branch2", secondExperimentBranches[1].getString("name"))
        assertEquals(7, secondExperimentBranches[1].getInt("ratio"))

        val secondExperimentMatch = secondExperiment.getJSONObject("match")
        val secondExperimentRegions = secondExperimentMatch.getJSONArray("regions")
        assertEquals(listOf("deu"), secondExperimentRegions.toList<String>())
        assertEquals("^org.mozilla.firefox${'$'}", secondExperimentMatch.getString("app_id"))
        assertEquals("es|deu", secondExperimentMatch.getString("locale_language"))
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