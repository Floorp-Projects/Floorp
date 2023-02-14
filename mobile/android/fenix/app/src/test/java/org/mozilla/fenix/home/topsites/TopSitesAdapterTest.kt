/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.topsites

import mozilla.components.feature.top.sites.TopSite
import org.junit.Assert.assertEquals
import org.junit.Test

class TopSitesAdapterTest {

    @Test
    fun testDiffCallback() {
        val topSite = TopSite.Default(
            id = 1L,
            title = "Title1",
            url = "https://mozilla.org",
            null,
        )
        val topSite2 = TopSite.Default(
            id = 1L,
            title = "Title2",
            url = "https://mozilla.org",
            null,
        )

        assertEquals(
            TopSitesAdapter.TopSitesDiffCallback.getChangePayload(topSite, topSite2),
            topSite.copy(title = "Title2"),
        )

        val topSite3 = TopSite.Default(
            id = 2L,
            title = "Title2",
            url = "https://firefox.org",
            null,
        )

        assertEquals(
            TopSitesAdapter.TopSitesDiffCallback.getChangePayload(topSite, topSite3),
            null,
        )
    }
}
