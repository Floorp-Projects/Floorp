/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.view.LayoutInflater
import io.mockk.mockk
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.databinding.ComponentTopSitesBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.sessioncontrol.TopSiteInteractor

@RunWith(FenixRobolectricTestRunner::class)
class TopSiteViewHolderTest {

    private lateinit var binding: ComponentTopSitesBinding
    private lateinit var interactor: TopSiteInteractor

    @Before
    fun setup() {
        binding = ComponentTopSitesBinding.inflate(LayoutInflater.from(testContext))
        interactor = mockk()
    }

    @Test
    fun `binds list of top sites`() {
        TopSiteViewHolder(binding.root, interactor).bind(
            listOf(
                TopSite(
                    id = 1L,
                    title = "Pocket",
                    url = "https://getpocket.com",
                    createdAt = 0,
                    type = TopSite.Type.DEFAULT
                )
            )
        )

        assertEquals(1, binding.topSitesList.adapter!!.itemCount)
    }
}
