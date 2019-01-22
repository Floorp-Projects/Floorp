/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session.bundling.adapter

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.session.bundling.db.BundleEntity
import mozilla.components.feature.session.bundling.db.UrlList
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
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

    @Test
    fun `Accessing id through adapter`() {
        val bundle = BundleEntity(42, "", 0, UrlList(listOf()))
        val adapter = SessionBundleAdapter(bundle)

        assertEquals(42L, adapter.id)
    }

    @Test
    fun `Accessing list of URLs through adapter`() {
        val bundle = BundleEntity(42, "", 0, UrlList(listOf(
            "https://www.mozilla.org",
            "https://www.firefox.com"
        )))

        val adapter = SessionBundleAdapter(bundle)

        assertEquals(2, adapter.urls.size)
        assertEquals("https://www.mozilla.org", adapter.urls[0])
        assertEquals("https://www.firefox.com", adapter.urls[1])
    }

    @Test
    fun `Accessing last save date through adapter`() {
        val bundle = BundleEntity(42, "", 1548165508982, UrlList(listOf()))

        val adapter = SessionBundleAdapter(bundle)

        assertEquals(1548165508982, adapter.lastSavedAt)
    }
}
