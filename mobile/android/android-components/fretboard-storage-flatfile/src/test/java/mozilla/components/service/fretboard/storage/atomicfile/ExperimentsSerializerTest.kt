/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard.storage.atomicfile

import mozilla.components.service.fretboard.Experiment
import org.json.JSONException
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

val experimentsJson = """
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
                                  ]
                              }
                              """

@RunWith(RobolectricTestRunner::class)
class ExperimentsSerializerTest {
    @Test
    fun testFromJsonValid() {
        val experiments = ExperimentsSerializer().fromJson(experimentsJson)
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
    }

    @Test(expected = JSONException::class)
    fun testFromJsonEmptyString() {
        ExperimentsSerializer().fromJson("")
    }

    @Test
    fun testFromJsonEmptyArray() {
        val experimentsJson = """ {"experiments":[]} """
        assertEquals(0, ExperimentsSerializer().fromJson(experimentsJson).size)
    }

    @Test
    fun testToJsonValid() {
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
        assertEquals(experimentsJson.replace("\\s+".toRegex(), ""), ExperimentsSerializer().toJson(experiments))
    }

    @Test
    fun testToJsonEmptyList() {
        val experiments = listOf<Experiment>()
        assertEquals("""{"experiments":[]}""", ExperimentsSerializer().toJson(experiments))
    }
}