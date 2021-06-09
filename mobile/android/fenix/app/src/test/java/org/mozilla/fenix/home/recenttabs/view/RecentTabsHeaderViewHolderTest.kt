/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recenttabs.view

import android.view.LayoutInflater
import android.view.View
import io.mockk.mockk
import io.mockk.verify
import kotlinx.android.synthetic.main.recent_tabs_header.view.*
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.sessioncontrol.SessionControlInteractor

@RunWith(FenixRobolectricTestRunner::class)
class RecentTabsHeaderViewHolderTest {

    private lateinit var view: View
    private lateinit var interactor: SessionControlInteractor

    @Before
    fun setup() {
        view = LayoutInflater.from(testContext).inflate(RecentTabsHeaderViewHolder.LAYOUT_ID, null)
        interactor = mockk(relaxed = true)
    }

    @Test
    fun `WHEN show all button is clicked THEN interactor iis called`() {
        RecentTabsHeaderViewHolder(view, interactor)

        view.show_all_button.performClick()

        verify { interactor.onRecentTabShowAllClicked() }
    }
}
