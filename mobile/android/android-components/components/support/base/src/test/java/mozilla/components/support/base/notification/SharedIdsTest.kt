/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.notification

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.base.ids.SharedIds
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

private const val TEST_SHARED_PREFERENCES_FILE = "mozilla.test.pref"
private const val NOTIFICATION_ID_LIFETIME: Long = 1000L * 60L * 60L * 24L * 7L
private const val ID_OFFSET = 10000

@RunWith(AndroidJUnit4::class)
class SharedIdsTest {

    @Test
    fun `Returns stable ids`() {
        val sharedIds = SharedIds(TEST_SHARED_PREFERENCES_FILE, NOTIFICATION_ID_LIFETIME, ID_OFFSET)
        val id = sharedIds.getIdForTag(testContext, "mozilla.test")

        assertEquals(ID_OFFSET, id)
        assertEquals(ID_OFFSET, sharedIds.getIdForTag(testContext, "mozilla.test"))
        assertEquals(ID_OFFSET, sharedIds.getIdForTag(testContext, "mozilla.test"))
    }

    @Test
    fun `Returns incrementing ids`() {
        val sharedIds = SharedIds(TEST_SHARED_PREFERENCES_FILE, NOTIFICATION_ID_LIFETIME, ID_OFFSET)
        val id = sharedIds.getNextIdForTag(testContext, "mozilla.test")

        assertEquals(ID_OFFSET, id)
        assertEquals(ID_OFFSET + 1, sharedIds.getNextIdForTag(testContext, "mozilla.test"))
        assertEquals(ID_OFFSET + 2, sharedIds.getNextIdForTag(testContext, "mozilla.test"))
    }

    @Test
    fun `Remove expired id`() {
        val sharedIds = SharedIds(TEST_SHARED_PREFERENCES_FILE, NOTIFICATION_ID_LIFETIME, ID_OFFSET)
        sharedIds.now = { 0 }
        val id = sharedIds.getIdForTag(testContext, "mozilla.test")

        assertEquals(ID_OFFSET, id)

        sharedIds.now = { NOTIFICATION_ID_LIFETIME + 1 }
        assertEquals(ID_OFFSET + 1, sharedIds.getIdForTag(testContext, "mozilla.test"))

        sharedIds.now = { (NOTIFICATION_ID_LIFETIME + 1) * 2 }
        assertEquals(ID_OFFSET + 2, sharedIds.getIdForTag(testContext, "mozilla.test"))
    }
}
