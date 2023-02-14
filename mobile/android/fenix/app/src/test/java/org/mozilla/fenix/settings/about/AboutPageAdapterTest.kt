/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.about

import android.view.ViewGroup
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.databinding.AboutListItemBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.about.viewholders.AboutItemViewHolder

@RunWith(FenixRobolectricTestRunner::class)
class AboutPageAdapterTest {
    private val aboutList: List<AboutPageItem> =
        listOf(
            AboutPageItem(
                AboutItem.ExternalLink(
                    AboutItemType.WHATS_NEW,
                    "https://mozilla.org",
                ),
                "Libraries",
            ),
            AboutPageItem(AboutItem.Libraries, "Libraries"),
            AboutPageItem(AboutItem.Crashes, "Crashes"),
        )
    private val listener: AboutPageListener = mockk(relaxed = true)

    @Test
    fun `getItemCount on a default instantiated Adapter should return 0`() {
        val adapter = AboutPageAdapter(listener)

        assertEquals(0, adapter.itemCount)
    }

    @Test
    fun `getItemCount after updateData() call should return the correct list size`() {
        val adapter = AboutPageAdapter(listener)

        adapter.submitList(aboutList)

        assertEquals(3, adapter.itemCount)
    }

    @Test
    fun `the adapter uses AboutItemViewHolder`() {
        val adapter = AboutPageAdapter(listener)
        val parentView: ViewGroup = mockk(relaxed = true)
        every { parentView.context } returns testContext

        val viewHolder = adapter.onCreateViewHolder(parentView, AboutItemViewHolder.LAYOUT_ID)

        assertEquals(AboutItemViewHolder::class, viewHolder::class)
    }

    @Test
    fun `the adapter binds the right item to a ViewHolder`() {
        val adapter = AboutPageAdapter(listener)
        val parentView: ViewGroup = mockk(relaxed = true)

        mockkStatic(AboutListItemBinding::class)
        val binding: AboutListItemBinding = mockk()

        every { AboutListItemBinding.bind(parentView) } returns binding
        every { binding.root } returns mockk()

        val viewHolder = spyk(AboutItemViewHolder(parentView, mockk()))

        every {
            adapter.onCreateViewHolder(
                parentView,
                AboutItemViewHolder.LAYOUT_ID,
            )
        } returns viewHolder

        every { viewHolder.bind(any()) } just Runs

        adapter.submitList(aboutList)
        adapter.bindViewHolder(viewHolder, 1)

        verify { viewHolder.bind(aboutList[1]) }
    }
}
