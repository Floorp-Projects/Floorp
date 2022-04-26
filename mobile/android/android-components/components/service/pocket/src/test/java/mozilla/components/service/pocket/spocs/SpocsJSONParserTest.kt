/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.spocs

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.service.pocket.helpers.PocketTestResources
import mozilla.components.service.pocket.helpers.assertClassVisibility
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.reflect.KVisibility

@RunWith(AndroidJUnit4::class)
class SpocsJSONParserTest {
    @Test
    fun `GIVEN a SpocsJSONParser THEN its visibility is internal`() {
        assertClassVisibility(SpocsJSONParser::class, KVisibility.INTERNAL)
    }

    @Test
    fun `GIVEN SpocsJSONParser WHEN parsing spocs THEN ApiSpocs are returned`() {
        val expectedSpocs = PocketTestResources.apiExpectedPocketSpocs
        val pocketJSON = PocketTestResources.pocketEndpointThreeSpocsResponse
        val actualSpocs = SpocsJSONParser.jsonToSpocs(pocketJSON)

        assertNotNull(actualSpocs)
        assertEquals(3, actualSpocs!!.size)
        assertEquals(expectedSpocs, actualSpocs)
    }

    @Test
    fun `WHEN parsing spocs with missing titles THEN those entries are dropped`() {
        val pocketJSON = PocketTestResources.pocketEndpointThreeSpocsResponse
        val expectedSpocsIfMissingTitle = ArrayList(PocketTestResources.apiExpectedPocketSpocs)
            .apply { removeAt(2) }
        val pocketJsonWithMissingTitle = removeJsonFieldFromArrayIndex("title", 2, pocketJSON)

        val result = SpocsJSONParser.jsonToSpocs(pocketJsonWithMissingTitle)

        assertEquals(2, result!!.size)
        assertEquals(expectedSpocsIfMissingTitle.joinToString(), result.joinToString())
    }

    @Test
    fun `WHEN parsing spocs with missing urls THEN those entries are dropped`() {
        val pocketJSON = PocketTestResources.pocketEndpointThreeSpocsResponse
        val expectedSpocsIfMissingTitle = ArrayList(PocketTestResources.apiExpectedPocketSpocs)
            .apply { removeAt(1) }
        val pocketJsonWithMissingTitle = removeJsonFieldFromArrayIndex("url", 1, pocketJSON)

        val result = SpocsJSONParser.jsonToSpocs(pocketJsonWithMissingTitle)

        assertEquals(2, result!!.size)
        assertEquals(expectedSpocsIfMissingTitle.joinToString(), result.joinToString())
    }

    @Test
    fun `WHEN parsing spocs with missing image urls THEN those entries are dropped`() {
        val pocketJSON = PocketTestResources.pocketEndpointThreeSpocsResponse
        val expectedSpocsIfMissingTitle = ArrayList(PocketTestResources.apiExpectedPocketSpocs)
            .apply { removeAt(0) }
        val pocketJsonWithMissingTitle = removeJsonFieldFromArrayIndex("image_src", 0, pocketJSON)

        val result = SpocsJSONParser.jsonToSpocs(pocketJsonWithMissingTitle)

        assertEquals(2, result!!.size)
        assertEquals(expectedSpocsIfMissingTitle.joinToString(), result.joinToString())
    }

    @Test
    fun `WHEN parsing spocs with missing sponsors THEN those entries are dropped`() {
        val pocketJSON = PocketTestResources.pocketEndpointThreeSpocsResponse
        val expectedSpocsIfMissingTitle = ArrayList(PocketTestResources.apiExpectedPocketSpocs)
            .apply { removeAt(1) }
        val pocketJsonWithMissingTitle = removeJsonFieldFromArrayIndex("sponsor", 1, pocketJSON)

        val result = SpocsJSONParser.jsonToSpocs(pocketJsonWithMissingTitle)

        assertEquals(2, result!!.size)
        assertEquals(expectedSpocsIfMissingTitle.joinToString(), result.joinToString())
    }

    @Test
    fun `WHEN parsing spocs with missing click shims THEN those entries are dropped`() {
        val pocketJSON = PocketTestResources.pocketEndpointThreeSpocsResponse
        val expectedSpocsIfMissingTitle = ArrayList(PocketTestResources.apiExpectedPocketSpocs)
            .apply { removeAt(2) }
        val pocketJsonWithMissingTitle = removeShimFromSpoc("click", 2, pocketJSON)

        val result = SpocsJSONParser.jsonToSpocs(pocketJsonWithMissingTitle)

        assertEquals(2, result!!.size)
        assertEquals(expectedSpocsIfMissingTitle.joinToString(), result.joinToString())
    }

    @Test
    fun `WHEN parsing spocs with missing impression shims THEN those entries are dropped`() {
        val pocketJSON = PocketTestResources.pocketEndpointThreeSpocsResponse
        val expectedSpocsIfMissingTitle = ArrayList(PocketTestResources.apiExpectedPocketSpocs)
            .apply { removeAt(1) }
        val pocketJsonWithMissingTitle = removeShimFromSpoc("impression", 1, pocketJSON)

        val result = SpocsJSONParser.jsonToSpocs(pocketJsonWithMissingTitle)

        assertEquals(2, result!!.size)
        assertEquals(expectedSpocsIfMissingTitle.joinToString(), result.joinToString())
    }

    @Test
    fun `WHEN parsing spocs for an invalid JSON String THEN null is returned`() {
        assertNull(SpocsJSONParser.jsonToSpocs("{!!}}"))
    }
}

private fun removeJsonFieldFromArrayIndex(fieldName: String, indexInArray: Int, json: String): String {
    val obj = JSONObject(json)
    val spocsJson = obj.getJSONArray(KEY_ARRAY_SPOCS)
    spocsJson.getJSONObject(indexInArray).remove(fieldName)
    return obj.toString()
}

private fun removeShimFromSpoc(shimName: String, spocIndex: Int, json: String): String {
    val obj = JSONObject(json)
    val spocsJson = obj.getJSONArray(KEY_ARRAY_SPOCS)
    val spocJson = spocsJson.getJSONObject(spocIndex)
    spocJson.getJSONObject(JSON_SPOC_SHIMS_KEY).remove(shimName)
    return obj.toString()
}
