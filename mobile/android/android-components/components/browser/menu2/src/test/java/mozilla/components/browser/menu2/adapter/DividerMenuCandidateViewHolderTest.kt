/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.adapter

import android.view.View
import mozilla.components.concept.menu.candidate.DividerMenuCandidate
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.verify

class DividerMenuCandidateViewHolderTest {

    @Test
    fun `sets visible and enabled state on divider view`() {
        val view: View = mock()
        val holder = DividerMenuCandidateViewHolder(view, mock())

        holder.bind(DividerMenuCandidate())
        verify(view).visibility = View.VISIBLE
        verify(view).isEnabled = true
    }
}
