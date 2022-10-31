/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.notification

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.base.ids.SharedIdsHelper
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class SharedIdsHelperTest {

    @After
    fun tearDown() {
        SharedIdsHelper.clear(testContext)
        SharedIdsHelper.now = { System.currentTimeMillis() }
    }

    @Test
    fun `Returns stable ids`() {
        SharedIdsHelper.now = { 0 }

        val testId = SharedIdsHelper.getIdForTag(testContext, "mozilla.test")
        assertTrue(testId > 0)
        assertTrue(testId > 1000)
        assertEquals(testId, SharedIdsHelper.getIdForTag(testContext, "mozilla.test"))
        assertEquals(testId, SharedIdsHelper.getIdForTag(testContext, "mozilla.test"))

        val anotherId = SharedIdsHelper.getIdForTag(testContext, "mozilla.test2")
        assertTrue(testId != anotherId)
        assertTrue(anotherId > testId)
    }

    @Test
    fun `Returns incrementing ids`() {
        SharedIdsHelper.now = { 0 }

        val testId = SharedIdsHelper.getNextIdForTag(testContext, "mozilla.test")
        assertTrue(testId > 0)
        assertTrue(testId > 1000)
        assertEquals(testId + 1, SharedIdsHelper.getNextIdForTag(testContext, "mozilla.test"))
        assertEquals(testId + 2, SharedIdsHelper.getNextIdForTag(testContext, "mozilla.test"))
    }

    @Test
    fun `Clears unused ids`() {
        SharedIdsHelper.now = { 0 }

        val firstId = SharedIdsHelper.getIdForTag(testContext, "mozilla.test")
        val secondId = SharedIdsHelper.getIdForTag(testContext, "mozilla.test.second")
        assertNotEquals(firstId, secondId)

        SharedIdsHelper.now = { 1000 * 60 * 60 * 24 * 2 }
        assertEquals(firstId, SharedIdsHelper.getIdForTag(testContext, "mozilla.test"))

        SharedIdsHelper.now = { 1000 * 60 * 60 * 24 * 8 }
        assertEquals(firstId, SharedIdsHelper.getIdForTag(testContext, "mozilla.test"))

        val updatedId = SharedIdsHelper.getIdForTag(testContext, "mozilla.test.second")
        assertNotEquals(firstId, updatedId)
        assertTrue(updatedId > firstId)
        assertTrue(updatedId > secondId)
    }
}
