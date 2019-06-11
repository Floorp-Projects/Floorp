/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.notification

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.base.ids.NotificationIds
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class NotificationIdsTest {

    @After
    fun tearDown() {
        NotificationIds.clear(testContext)
        NotificationIds.now = { System.currentTimeMillis() }
    }

    @Test
    fun `Returns stable ids`() {
        NotificationIds.now = { 0 }

        val testId = NotificationIds.getIdForTag(testContext, "mozilla.test")
        assertTrue(testId > 0)
        assertTrue(testId > 1000)
        assertEquals(testId, NotificationIds.getIdForTag(testContext, "mozilla.test"))
        assertEquals(testId, NotificationIds.getIdForTag(testContext, "mozilla.test"))

        val anotherId = NotificationIds.getIdForTag(testContext, "mozilla.test2")
        assertTrue(testId != anotherId)
        assertTrue(anotherId > testId)
    }

    @Test
    fun `Clears unused ids`() {
        NotificationIds.now = { 0 }

        val firstId = NotificationIds.getIdForTag(testContext, "mozilla.test")
        val secondId = NotificationIds.getIdForTag(testContext, "mozilla.test.second")
        assertNotEquals(firstId, secondId)

        NotificationIds.now = { 1000 * 60 * 60 * 24 * 2 }
        assertEquals(firstId, NotificationIds.getIdForTag(testContext, "mozilla.test"))

        NotificationIds.now = { 1000 * 60 * 60 * 24 * 8 }
        assertEquals(firstId, NotificationIds.getIdForTag(testContext, "mozilla.test"))

        val updatedId = NotificationIds.getIdForTag(testContext, "mozilla.test.second")
        assertNotEquals(firstId, updatedId)
        assertTrue(updatedId > firstId)
        assertTrue(updatedId > secondId)
    }
}
