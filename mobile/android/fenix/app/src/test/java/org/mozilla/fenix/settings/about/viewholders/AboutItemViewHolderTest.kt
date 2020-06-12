/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.about.viewholders

import android.view.LayoutInflater
import android.view.View
import io.mockk.mockk
import io.mockk.verify
import kotlinx.android.synthetic.main.about_list_item.view.*
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.about.AboutItem
import org.mozilla.fenix.settings.about.AboutPageItem
import org.mozilla.fenix.settings.about.AboutPageListener

@RunWith(FenixRobolectricTestRunner::class)
class AboutItemViewHolderTest {

    private val item = AboutPageItem.Item(AboutItem.Libraries, "Libraries")
    private lateinit var view: View
    private lateinit var listener: AboutPageListener

    @Before
    fun setup() {
        view = LayoutInflater.from(testContext).inflate(AboutItemViewHolder.LAYOUT_ID, null)
        listener = mockk(relaxed = true)
    }

    @Test
    fun `bind title`() {
        val holder = AboutItemViewHolder(view, listener)
        holder.bind(item)

        assertEquals("Libraries", view.about_item_title.text)
    }

    @Test
    fun `call listener on click`() {
        val holder = AboutItemViewHolder(view, listener)
        holder.bind(item)
        view.performClick()

        verify { listener.onAboutItemClicked(AboutItem.Libraries) }
    }
}
