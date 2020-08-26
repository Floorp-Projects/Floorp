/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.topsites

import android.view.LayoutInflater
import android.view.View
import io.mockk.mockk
import io.mockk.verify
import kotlinx.android.synthetic.main.top_site_item.view.*
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.sessioncontrol.TopSiteInteractor

@RunWith(FenixRobolectricTestRunner::class)
class TopSiteItemViewHolderTest {

    private lateinit var view: View
    private lateinit var interactor: TopSiteInteractor
    private val pocket = TopSite(
        id = 1L,
        title = "Pocket",
        url = "https://getpocket.com",
        createdAt = 0,
        type = TopSite.Type.DEFAULT
    )

    @Before
    fun setup() {
        view = LayoutInflater.from(testContext)
            .inflate(TopSiteItemViewHolder.LAYOUT_ID, null)
        interactor = mockk(relaxed = true)
    }

    @Test
    fun `calls interactor on click`() {
        TopSiteItemViewHolder(view, interactor).bind(pocket)

        view.top_site_item.performClick()
        verify { interactor.onSelectTopSite("https://getpocket.com", isDefault = true) }
    }
}
