/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabhistory

import android.content.Context
import android.widget.FrameLayout
import androidx.appcompat.view.ContextThemeWrapper
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class TabHistoryAdapterTest {

    @MockK
    private lateinit var interactor: TabHistoryInteractor
    private lateinit var context: Context
    private lateinit var parent: FrameLayout
    private lateinit var adapter: TabHistoryAdapter

    private val selectedItem = TabHistoryItem(
        title = "Mozilla",
        url = "https://mozilla.org",
        index = 0,
        isSelected = true,
    )
    private val unselectedItem = TabHistoryItem(
        title = "Firefox",
        url = "https://firefox.com",
        index = 1,
        isSelected = false,
    )

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        context = ContextThemeWrapper(testContext, R.style.NormalTheme)
        parent = FrameLayout(context)
        adapter = TabHistoryAdapter(interactor)
    }

    @Test
    fun `creates and binds view holder`() {
        every { testContext.components.core.icons } returns BrowserIcons(testContext, mockk(relaxed = true))
        adapter.submitList(listOf(selectedItem, unselectedItem))

        val holder = spyk(adapter.createViewHolder(parent, 0))

        adapter.bindViewHolder(holder, 0)
        verify { holder.bind(selectedItem) }

        adapter.bindViewHolder(holder, 1)
        verify { holder.bind(unselectedItem) }
    }

    @Test
    fun `items are the same if they have matching URLs`() {
        assertTrue(
            TabHistoryAdapter.DiffCallback.areItemsTheSame(
                selectedItem,
                selectedItem,
            ),
        )
        assertTrue(
            TabHistoryAdapter.DiffCallback.areItemsTheSame(
                unselectedItem,
                unselectedItem.copy(title = "Waterbug", index = 2, isSelected = true),
            ),
        )
        assertFalse(
            TabHistoryAdapter.DiffCallback.areItemsTheSame(
                unselectedItem,
                unselectedItem.copy(url = "https://firefox.com/subpage"),
            ),
        )
    }

    @Test
    fun `equal items have the same contents`() {
        assertTrue(
            TabHistoryAdapter.DiffCallback.areContentsTheSame(
                selectedItem,
                selectedItem,
            ),
        )
        assertFalse(
            TabHistoryAdapter.DiffCallback.areContentsTheSame(
                selectedItem,
                selectedItem.copy(title = "Waterbug", index = 2, isSelected = false),
            ),
        )
        assertFalse(
            TabHistoryAdapter.DiffCallback.areContentsTheSame(
                unselectedItem,
                selectedItem,
            ),
        )
    }
}
