/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu

import android.view.View
import android.widget.ImageButton
import androidx.recyclerview.widget.RecyclerView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class BrowserMenuBuilderTest {

    @Test
    fun `items are forwarded from builder to menu`() {
        val builder = BrowserMenuBuilder(listOf(mockMenuItem(), mockMenuItem()))

        val menu = builder.build(testContext)

        val anchor = ImageButton(testContext)
        val popup = menu.show(anchor)

        val recyclerView: RecyclerView = popup.contentView.findViewById(R.id.mozac_browser_menu_recyclerView)
        assertNotNull(recyclerView)

        val recyclerAdapter = recyclerView.adapter!!
        assertNotNull(recyclerAdapter)
        assertEquals(2, recyclerAdapter.itemCount)
    }

    private fun mockMenuItem() = object : BrowserMenuItem {
        override val visible: () -> Boolean = { true }

        override fun getLayoutResource() = R.layout.mozac_browser_menu_item_simple

        override fun bind(menu: BrowserMenu, view: View) {}
    }
}
