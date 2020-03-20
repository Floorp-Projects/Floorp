/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.about

import android.view.ViewGroup
import android.widget.TextView
import assertk.assertThat
import assertk.assertions.isEqualTo
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import kotlinx.android.synthetic.main.about_list_item.view.*
import mozilla.components.support.test.robolectric.testContext
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.TestApplication
import org.mozilla.fenix.settings.about.viewholders.AboutItemViewHolder
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)
class AboutPageAdapterTest {
    private var aboutList: List<AboutPageItem> =
        mutableListOf(
            AboutPageItem.Item(
                AboutItem.ExternalLink(
                    AboutItemType.WHATS_NEW,
                    "https://mozilla.org"
                ), "Libraries"
            ),
            AboutPageItem.Item(AboutItem.Libraries, "Libraries")
        )
    private val listener: AboutPageListener = mockk(relaxed = true)

    @Test
    fun `getItemCount on a default instantiated Adapter should return 0`() {
        val adapter = AboutPageAdapter(listener)

        assertThat(adapter.itemCount).isEqualTo(0)
    }

    @Test
    fun `getItemCount after updateData() call should return the correct list size`() {
        val adapter = AboutPageAdapter(listener)

        adapter.submitList(aboutList)

        assertThat(adapter.itemCount).isEqualTo(2)
    }

    @Test
    fun `the adapter uses AboutItemViewHolder`() {
        val adapter = AboutPageAdapter(listener)
        val parentView: ViewGroup = mockk(relaxed = true)
        every { parentView.context } returns testContext

        val viewHolder = adapter.onCreateViewHolder(parentView, AboutItemViewHolder.LAYOUT_ID)

        assertThat(viewHolder::class).isEqualTo(AboutItemViewHolder::class)
    }

    @Test
    fun `the adapter binds the right item to a ViewHolder`() {
        val adapter = AboutPageAdapter(listener)
        val parentView: ViewGroup = mockk(relaxed = true)
        every { parentView.about_item_title } returns TextView(testContext)
        val viewHolder = spyk(AboutItemViewHolder(parentView, mockk()))
        every {
            adapter.onCreateViewHolder(
                parentView,
                AboutItemViewHolder.LAYOUT_ID
            )
        } returns viewHolder
        every { viewHolder.bind(any()) } just Runs

        adapter.submitList(aboutList)
        adapter.bindViewHolder(viewHolder, 1)

        verify { viewHolder.bind(aboutList[1] as AboutPageItem.Item) }
    }
}
