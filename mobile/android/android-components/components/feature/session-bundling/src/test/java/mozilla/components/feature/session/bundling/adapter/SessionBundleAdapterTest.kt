/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session.bundling.adapter

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.session.bundling.db.BundleEntity
import mozilla.components.feature.session.bundling.db.UrlList
import mozilla.components.support.test.mock
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class SessionBundleAdapterTest {
    @Test
    fun `restoreSnapshot restores snapshot from state`() {
        val bundle = BundleEntity(0, "", 0, UrlList(listOf()))

        val snapshot = SessionManager.Snapshot(
            listOf(SessionManager.Snapshot.Item(session = Session("https://www.mozilla.org"))),
            selectedSessionIndex = 0)

        bundle.updateFrom(snapshot)

        val restoredSnapshot = SessionBundleAdapter(bundle).restoreSnapshot(mock())

        assertNotNull(restoredSnapshot!!)

        assertFalse(restoredSnapshot.isEmpty())
        assertEquals(1, restoredSnapshot.sessions.size)
        assertEquals("https://www.mozilla.org", restoredSnapshot.sessions[0].session.url)
    }
}