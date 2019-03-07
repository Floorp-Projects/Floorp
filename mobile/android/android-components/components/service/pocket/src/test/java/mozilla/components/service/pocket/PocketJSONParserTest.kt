/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

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
import org.robolectric.RobolectricTestRunner

private const val TEST_DATA_SIZE = 20

private const val KEY_AUTHORS = "authors"

private val TEST_AUTHOR = PocketGlobalVideoRecommendation.Author(
    id = "101",
    name = "The New York Times",
    url = "https://www.nytimes.com/"
)

@RunWith(RobolectricTestRunner::class)
class PocketJSONParserTest {

    private lateinit var parser: PocketJSONParser

    @Before
    fun setUp() {
        parser = PocketJSONParser()
    }

    @Test
    fun `WHEN parsing valid global video recommendations THEN pocket videos are returned`() {
        val expectedSubset = listOf(
            PocketGlobalVideoRecommendation(
                id = 27587,
                url = "https://www.youtube.com/watch?v=953Qt4FnAcU",
                tvURL = "https://www.youtube.com/tv#/watch/video/idle?v=953Qt4FnAcU",
                title = "How a Master Pastry Chef Uses Architecture to Make Sky High Pastries",
                excerpt = "At New York City's International Culinary Center, host Rebecca DeAngelis makes a modern day croquembouche with architect-turned pastry chef Jansen Chan",
                domain = "youtube.com",
                imageSrc = "https://img-getpocket.cdn.mozilla.net/direct?url=http%3A%2F%2Fimg.youtube.com%2Fvi%2F953Qt4FnAcU%2Fmaxresdefault.jpg&resize=w450",
                publishedTimestamp = "0",
                sortId = 0,
                popularitySortId = 20,
                authors = listOf(PocketGlobalVideoRecommendation.Author(
                    id = "96612022",
                    name = "Eater",
                    url = "http://www.youtube.com/channel/UCRzPUBhXUZHclB7B5bURFXw"
                ))
            ),
            PocketGlobalVideoRecommendation(
                id = 27581,
                url = "https://www.youtube.com/watch?v=GHZ7-kq3GDQ",
                tvURL = "https://www.youtube.com/tv#/watch/video/idle?v=GHZ7-kq3GDQ",
                title = "How Does Having Too Much Power Affect Your Brain?",
                excerpt = "Power is tied to a hierarchical escalator that we rise up through decisions and actions. But once we have power, how does it affect our brain and behavior? How Our Brains Respond to People Who Aren't Like Us - https://youtu.be/KIwe_O0am4URead More:The age of adolescencehttps://www.thelancet.com/jour",
                domain = "youtube.com",
                imageSrc = "https://img-getpocket.cdn.mozilla.net/direct?url=http%3A%2F%2Fimg.youtube.com%2Fvi%2FGHZ7-kq3GDQ%2Fmaxresdefault.jpg&resize=w450",
                publishedTimestamp = "0",
                sortId = 1,
                popularitySortId = 17,
                authors = listOf(PocketGlobalVideoRecommendation.Author(
                    id = "96612138",
                    name = "Seeker",
                    url = "http://www.youtube.com/channel/UCzWQYUVCpZqtN93H8RR44Qw"
                ))
            )
        )

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
