/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recenttabs.view

import android.view.LayoutInflater
import android.view.View
import androidx.core.graphics.drawable.toBitmap
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import kotlinx.android.synthetic.main.recent_tabs_list_row.*
import kotlinx.android.synthetic.main.recent_tabs_list_row.view.*
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.IconRequest
import mozilla.components.browser.state.state.createTab
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.sessioncontrol.SessionControlInteractor

@RunWith(FenixRobolectricTestRunner::class)
class RecentTabViewHolderTest {

    private lateinit var view: View
    private lateinit var interactor: SessionControlInteractor
    private lateinit var icons: BrowserIcons

    private val tab = createTab(
        url = "https://mozilla.org",
        title = "Mozilla"
    )

    @Before
    fun setup() {
        view = LayoutInflater.from(testContext).inflate(RecentTabViewHolder.LAYOUT_ID, null)
        interactor = mockk(relaxed = true)
        icons = mockk(relaxed = true)

        every { icons.loadIntoView(view.recent_tab_icon, any()) } returns mockk()
    }

    @Test
    fun `GIVEN a new recent tab on bind THEN set the title text and load the tab icon`() {
        RecentTabViewHolder(view, interactor, icons).bindTab(tab)

        assertEquals(tab.content.title, view.recent_tab_title.text)

        verify { icons.loadIntoView(view.recent_tab_icon, IconRequest(tab.content.url)) }
    }

    @Test
    fun `WHEN a recent tab item is clicked THEN interactor is called`() {
        RecentTabViewHolder(view, interactor, icons).bindTab(tab)

        view.performClick()

        verify { interactor.onRecentTabClicked(tab.id) }
    }

    @Test
    fun `WHEN a recent tab icon exists THEN load it`() {
        val bitmap = testContext.getDrawable(R.drawable.ic_search)!!.toBitmap()
        val tabWithIcon = tab.copy(content = tab.content.copy(icon = bitmap))
        val viewHolder = RecentTabViewHolder(view, interactor, icons)

        assertNull(view.recent_tab_icon.drawable)

        viewHolder.bindTab(tabWithIcon)

        assertNotNull(view.recent_tab_icon.drawable)
    }

    @Test
    fun `WHEN a recent tab does not have a title THEN show the url`() {
        val tabWithoutTitle = createTab(url = "https://mozilla.org")

        RecentTabViewHolder(view, interactor, icons).bindTab(tabWithoutTitle)

        assertEquals(tabWithoutTitle.content.url, view.recent_tab_title.text)
    }
}
