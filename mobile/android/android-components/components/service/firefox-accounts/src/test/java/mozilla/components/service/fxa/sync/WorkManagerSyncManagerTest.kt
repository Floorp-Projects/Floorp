/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.sync

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class WorkManagerSyncManagerTest {
    @Test
    fun `sync state access`() {
        assertNull(getSyncState(testContext))
        assertEquals(0L, getLastSynced(testContext))

        // 'clear' doesn't blow up for empty state
        clearSyncState(testContext)
        // ... and doesn't affect anything, either
        assertNull(getSyncState(testContext))
        assertEquals(0L, getLastSynced(testContext))

        setSyncState(testContext, "some state")
        assertEquals("some state", getSyncState(testContext))

        setLastSynced(testContext, 123L)
        assertEquals(123L, getLastSynced(testContext))

        clearSyncState(testContext)
        assertNull(getSyncState(testContext))
        assertEquals(0L, getLastSynced(testContext))
    }
}