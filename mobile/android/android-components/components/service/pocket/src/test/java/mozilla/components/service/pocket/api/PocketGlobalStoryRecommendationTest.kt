/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.api

import mozilla.components.service.pocket.helpers.assertClassVisibility
import mozilla.components.service.pocket.helpers.assertConstructorsVisibility
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import kotlin.reflect.KVisibility
import kotlin.reflect.full.createType
import kotlin.reflect.full.memberProperties

class PocketGlobalStoryRecommendationTest {

    @Test
    fun `GIVEN a PocketGlobalStoryRecommendation THEN its constructors are internal`() {
        assertConstructorsVisibility(PocketGlobalStoryRecommendation::class, KVisibility.INTERNAL)
    }

    @Test
    fun `GIVEN a PocketGlobalStoryRecommendation THEN its visibility is internal`() {
        assertClassVisibility(PocketGlobalStoryRecommendation::class, KVisibility.INTERNAL)
    }

    @Test
    fun `GIVEN a PocketGlobalStoryRecommendation THEN it has a specific list of properties`() {
        val story: PocketGlobalStoryRecommendation = mock()
        val storyProperties = story::class::memberProperties.get()

        assertEquals(10, storyProperties.size)
        assertTrue(storyProperties.find {
            it.name == "id"
        }!!.returnType == Long::class.createType())
        assertTrue(storyProperties.find {
            it.name == "url"
        }!!.returnType == String::class.createType())
        assertTrue(storyProperties.find {
            it.name == "domain"
        }!!.returnType == String::class.createType())
        assertTrue(storyProperties.find {
            it.name == "title"
        }!!.returnType == String::class.createType())
        assertTrue(storyProperties.find {
            it.name == "excerpt"
        }!!.returnType == String::class.createType())
        assertTrue(storyProperties.find {
            it.name == "imageSrc"
        }!!.returnType == String::class.createType())
        assertTrue(storyProperties.find {
            it.name == "publishedTimestamp"
        }!!.returnType == String::class.createType())
        assertTrue(storyProperties.find {
            it.name == "engagement"
        }!!.returnType == String::class.createType())
        assertTrue(storyProperties.find {
            it.name == "dedupeUrl"
        }!!.returnType == String::class.createType())
        assertTrue(storyProperties.find {
            it.name == "sortId"
        }!!.returnType == Int::class.createType())
    }
}
