/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.adapter

import android.view.LayoutInflater
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.menu.candidate.CompoundMenuCandidate
import mozilla.components.concept.menu.candidate.DecorativeTextMenuCandidate
import mozilla.components.concept.menu.candidate.DividerMenuCandidate
import mozilla.components.concept.menu.candidate.NestedMenuCandidate
import mozilla.components.concept.menu.candidate.RowMenuCandidate
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class MenuCandidateListAdapterTest {

    private lateinit var layoutInflater: LayoutInflater
    private lateinit var dismiss: () -> Unit
    private lateinit var reopenMenu: (NestedMenuCandidate) -> Unit
    private lateinit var adapter: MenuCandidateListAdapter

    @Before
    fun setup() {
        layoutInflater = spy(LayoutInflater.from(testContext))
        dismiss = mock()
        reopenMenu = mock()
        adapter = MenuCandidateListAdapter(layoutInflater, dismiss, reopenMenu)
    }

    @Test
    fun `items use layout resource as view type`() {
        val items = listOf(
            DecorativeTextMenuCandidate("one"),
            TextMenuCandidate("two"),
            CompoundMenuCandidate("three", false, end = CompoundMenuCandidate.ButtonType.CHECKBOX),
            CompoundMenuCandidate("four", false, end = CompoundMenuCandidate.ButtonType.SWITCH),
            DividerMenuCandidate(),
            RowMenuCandidate(emptyList()),
        )
        adapter.submitList(items)

        assertEquals(6, adapter.itemCount)
        assertEquals(DecorativeTextMenuCandidateViewHolder.layoutResource, adapter.getItemViewType(0))
        assertEquals(TextMenuCandidateViewHolder.layoutResource, adapter.getItemViewType(1))
        assertEquals(CompoundCheckboxMenuCandidateViewHolder.layoutResource, adapter.getItemViewType(2))
        assertEquals(CompoundSwitchMenuCandidateViewHolder.layoutResource, adapter.getItemViewType(3))
        assertEquals(DividerMenuCandidateViewHolder.layoutResource, adapter.getItemViewType(4))
        assertEquals(RowMenuCandidateViewHolder.layoutResource, adapter.getItemViewType(5))
    }

    @Test
    fun `bind will be forwarded to item implementation`() {
        adapter.submitList(
            listOf(
                DividerMenuCandidate(),
            ),
        )

        val holder: DividerMenuCandidateViewHolder = mock()

        adapter.onBindViewHolder(holder, 0)

        verify(holder).bind(DividerMenuCandidate())
    }
}
