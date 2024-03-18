/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.manager

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class SyncEnginesStorageTest {
    @Test
    fun `sync engine storage basics`() {
        val store = SyncEnginesStorage(testContext)
        assertEquals(emptyMap<SyncEngine, Boolean>(), store.getStatus())

        store.setStatus(SyncEngine.Bookmarks, false)
        assertEquals(mapOf(SyncEngine.Bookmarks to false), store.getStatus())

        store.setStatus(SyncEngine.Bookmarks, true)
        assertEquals(mapOf(SyncEngine.Bookmarks to true), store.getStatus())

        store.setStatus(SyncEngine.Passwords, false)
        assertEquals(mapOf(SyncEngine.Bookmarks to true, SyncEngine.Passwords to false), store.getStatus())

        store.setStatus(SyncEngine.Bookmarks, false)
        assertEquals(mapOf(SyncEngine.Bookmarks to false, SyncEngine.Passwords to false), store.getStatus())

        store.setStatus(SyncEngine.Other("test"), true)
        assertEquals(
            mapOf(
                SyncEngine.Bookmarks to false,
                SyncEngine.Passwords to false,
                SyncEngine.Other("test") to true,
            ),
            store.getStatus(),
        )

        store.clear()
        assertTrue(store.getStatus().isNullOrEmpty())
    }
}
