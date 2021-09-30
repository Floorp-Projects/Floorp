/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.view.LayoutInflater
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.tabstray.TabsTrayInteractor
import org.mozilla.fenix.tabstray.browser.InactiveTabViewHolder.HeaderHolder

@RunWith(FenixRobolectricTestRunner::class)
class InactiveTabViewHolderTest {
    @Test
    fun `HeaderHolder - WHEN clicked THEN notify the interactor`() {
        val view = LayoutInflater.from(testContext).inflate(HeaderHolder.LAYOUT_ID, null)
        val interactor: InactiveTabsInteractor = mockk(relaxed = true)
        val tabsTrayInteractor: TabsTrayInteractor = mockk(relaxed = true)
        val viewHolder = HeaderHolder(view, interactor, tabsTrayInteractor)

        val initialActivatedState = view.isActivated

        viewHolder.itemView.performClick()

        verify { interactor.onHeaderClicked(any()) }

        assertEquals(!initialActivatedState, view.isActivated)
    }
}
