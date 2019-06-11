/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.service.pocket.PocketJSONParser.Companion.KEY_VIDEO_RECOMMENDATIONS_INNER
import mozilla.components.service.pocket.data.PocketGlobalVideoRecommendation
import mozilla.components.service.pocket.helpers.PocketTestResource
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

private const val TEST_DATA_SIZE = 20

private const val KEY_AUTHORS = "authors"

private val TEST_AUTHOR = PocketGlobalVideoRecommendation.Author(
    id = "101",
    name = "The New York Times",
    url = "https://www.nytimes.com/"
)

@RunWith(AndroidJUnit4::class)
class PocketJSONParserTest {

    private lateinit var parser: PocketJSONParser

    @Before
    fun setUp() {
        parser = PocketJSONParser()
    }

    @Test
    fun `WHEN parsing valid global video recommendations THEN pocket videos are returned`() {
        val expectedSubset = PocketTestResource.getExpectedPocketVideoRecommendationFirstTwo()
        val pocketJSON = PocketTestResource.POCKET_VIDEO_RECOMMENDATION.get()
        val actualVideos = parser.jsonToGlobalVideoRecommendations(pocketJSON)

        // We only test a subset of the data for developer sanity. :)
        assertNotNull(actualVideos)
        assertEquals(TEST_DATA_SIZE, actualVideos!!.size)
        expectedSubset.forEachIndexed { i, expected ->
            assertEquals(expected, actualVideos[i])
        }
    }

    @Test
    fun `WHEN parsing global video recommendations with no authors THEN the returned authors is an empty list`() {
        val pocketJSON = PocketTestResource.POCKET_VIDEO_RECOMMENDATION.get()
        val jsonFirstMissingAuthors = replaceFirstAuthorsList(pocketJSON, JSONObject())

        val actualVideos = parser.jsonToGlobalVideoRecommendations(jsonFirstMissingAuthors)
        assertNotNull(actualVideos)
        assertEquals(emptyList<PocketGlobalVideoRecommendation.Author>(), actualVideos!![0].authors)
    }

    @Test
    fun `WHEN parsing global video recommendations with multiple authors THEN there are multiple returned authors`() {
        val expectedAuthors = listOf(
            TEST_AUTHOR,
            PocketGlobalVideoRecommendation.Author(
                id = "200",
                name = "Vox",
                url = "https://www.vox.com/"
            ))

        val newAuthors = JSONObject().apply {
            expectedAuthors.forEach { expectedAuthor ->
                val newAuthorJSON = expectedAuthor.toJSONObject()
                put(expectedAuthor.id, newAuthorJSON)
            }
        }

        val pocketJSON = PocketTestResource.POCKET_VIDEO_RECOMMENDATION.get()
        val jsonFirstMultipleAuthors = replaceFirstAuthorsList(pocketJSON, newAuthors)

        val actual = parser.jsonToGlobalVideoRecommendations(jsonFirstMultipleAuthors)
        assertNotNull(actual)
        assertEquals(expectedAuthors, actual!![0].authors)
    }

    @Test
    fun `WHEN parsing global video recommendations with some invalid authors THEN the authors are returned without invalid entries`() {
        val expectedAuthors = listOf(TEST_AUTHOR)
        val newAuthors = JSONObject().apply {
            val expectedAuthor = expectedAuthors[0]
            put(expectedAuthor.id, expectedAuthor.toJSONObject())
            put("999", JSONObject().apply { put("id", "999") })
        }

        val pocketJSON = PocketTestResource.POCKET_VIDEO_RECOMMENDATION.get()
        val jsonFirstInvalidAuthors = replaceFirstAuthorsList(pocketJSON, newAuthors)

        val actual = parser.jsonToGlobalVideoRecommendations(jsonFirstInvalidAuthors)
        assertNotNull(actual)
        assertEquals(TEST_DATA_SIZE, actual!!.size)
        assertEquals(expectedAuthors, actual[0].authors)
    }

    @Test
    fun `WHEN parsing global video recommendations with missing fields on some items THEN those entries are dropped`() {
        val pocketJSON = PocketTestResource.POCKET_VIDEO_RECOMMENDATION.get()

        val expectedFirstTitle = JSONObject(pocketJSON)
            .getJSONArray(KEY_VIDEO_RECOMMENDATIONS_INNER)
            .getJSONObject(0)
            .getString("title")
        assertNotNull(expectedFirstTitle)

        val pocketJSONWithNoTitleExceptFirst = removeTitleStartingAtIndex(1, pocketJSON)
        val actualVideos = parser.jsonToGlobalVideoRecommendations(pocketJSONWithNoTitleExceptFirst)

        assertNotNull(actualVideos)
        assertEquals(1, actualVideos!!.size)
        assertEquals(expectedFirstTitle, actualVideos[0].title)
    }

    @Test
    fun `WHEN parsing global video recommendations with missing fields on all items THEN null is returned`() {
        val pocketJSON = PocketTestResource.POCKET_VIDEO_RECOMMENDATION.get()
        val pocketJSONWithNoTitles = removeTitleStartingAtIndex(0, pocketJSON)
        val actualVideos = parser.jsonToGlobalVideoRecommendations(pocketJSONWithNoTitles)
        assertNull(actualVideos)
    }

    @Test
    fun `WHEN parsing global video recommendations for an empty string THEN null is returned`() {
        assertNull(parser.jsonToGlobalVideoRecommendations(""))
    }

    @Test
    fun `WHEN parsing global video recommendations for an invalid JSON String THEN null is returned`() {
        assertNull(parser.jsonToGlobalVideoRecommendations("{!!}}"))
    }

    @Test
    fun `WHEN newInstance is called THEN no exception is thrown`() {
        PocketJSONParser.newInstance()
    }
}

private fun replaceFirstAuthorsList(fullJSONStr: String, replacedAuthors: JSONObject): String {
    val json = JSONObject(fullJSONStr)
    val firstRecommendation = json.getJSONArray(KEY_VIDEO_RECOMMENDATIONS_INNER).getJSONObject(0)
    firstRecommendation.put(KEY_AUTHORS, replacedAuthors)
    return json.toString()
}

private fun removeTitleStartingAtIndex(startIndex: Int, json: String): String {
    val obj = JSONObject(json)
    val videosJson = obj.getJSONArray(KEY_VIDEO_RECOMMENDATIONS_INNER)
    for (i in startIndex until videosJson.length()) {
        videosJson.getJSONObject(i).remove("title")
    }
    return obj.toString()
}

private fun PocketGlobalVideoRecommendation.Author.toJSONObject(): JSONObject = JSONObject().also {
    it.put("author_id", id)
    it.put("name", name)
    it.put("url", url)
}
