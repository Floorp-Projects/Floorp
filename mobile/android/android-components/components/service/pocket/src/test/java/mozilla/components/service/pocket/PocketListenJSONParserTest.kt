/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.service.pocket.helpers.PocketTestResource
import org.json.JSONArray
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class PocketListenJSONParserTest {

    private lateinit var subject: PocketListenJSONParser

    @Before
    fun setUp() {
        subject = PocketListenJSONParser()
    }

    @Test
    fun `WHEN parsing listen article metadata for a blank string THEN null is returned`() {
        assertNull(subject.jsonToListenArticleMetadata(" "))
    }

    @Test
    fun `WHEN parsing listen article metadata for an invalid JSON String THEN null is returned`() {
        assertNull(subject.jsonToListenArticleMetadata("{!!}}"))
    }

    @Test
    fun `WHEN parsing listen article metadata with missing fields THEN null is returned`() {
        val modifiedPocketJSON = PocketTestResource.LISTEN_ARTICLE_METADATA.get().let {
            val rootJSON = JSONArray(it)
            rootJSON.getJSONObject(0).remove("voice")
            rootJSON.toString()
        }
        assertNull(subject.jsonToListenArticleMetadata(modifiedPocketJSON))
    }

    @Test
    fun `WHEN parsing valid listen article metadata THEN the article metadata is returned`() {
        val pocketJSON = PocketTestResource.LISTEN_ARTICLE_METADATA.get()
        val actual = subject.jsonToListenArticleMetadata(pocketJSON)
        assertEquals(PocketTestResource.getExpectedListenArticleMetadata(), actual)
    }
}
