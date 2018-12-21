/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session.bundling.db

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.support.test.mock
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class BundleEntityTest {
    @Test
    fun `updateFrom updates state and time`() {
        val bundle = BundleEntity(0, "", 0)

        val snapshot = SessionManager.Snapshot(
            listOf(SessionManager.Snapshot.Item(session = Session("https://www.mozilla.org"))),
            selectedSessionIndex = 0)

        bundle.updateFrom(snapshot)

        assertTrue(bundle.savedAt > 0)

        val json = JSONObject(bundle.state)

        assertEquals(1, json.get("version"))
        assertEquals(0, json.get("selectedSessionIndex"))

        val sessions = json.getJSONArray("sessionStateTuples")
        assertEquals(1, sessions.length())
    }

    @Test
    fun `restoreSnapshot restores snapshot from state`() {
        val bundle = BundleEntity(0, "", 0)

        val snapshot = SessionManager.Snapshot(
            listOf(SessionManager.Snapshot.Item(session = Session("https://www.mozilla.org"))),
            selectedSessionIndex = 0)

        bundle.updateFrom(snapshot)

        val restoredSnapshot = bundle.restoreSnapshot(mock())

        assertNotNull(restoredSnapshot!!)

        assertFalse(restoredSnapshot.isEmpty())
        assertEquals(1, restoredSnapshot.sessions.size)
        assertEquals("https://www.mozilla.org", restoredSnapshot.sessions[0].session.url)
    }
}
