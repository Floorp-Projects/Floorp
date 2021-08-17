/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recenttabs.view

import android.view.LayoutInflater
import androidx.core.graphics.drawable.toBitmap
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
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
import org.mozilla.fenix.databinding.RecentTabsListRowBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.sessioncontrol.SessionControlInteractor

@RunWith(FenixRobolectricTestRunner::class)
class RecentTabViewHolderTest {

    private lateinit var binding: RecentTabsListRowBinding
    private lateinit var interactor: SessionControlInteractor
    private lateinit var icons: BrowserIcons

    private val tab = createTab(
        url = "https://mozilla.org",
        title = "Mozilla"
    )

    @Before
    fun setup() {
        binding = RecentTabsListRowBinding.inflate(LayoutInflater.from(testContext))
        interactor = mockk(relaxed = true)
        icons = mockk(relaxed = true)

        every { icons.loadIntoView(binding.recentTabIcon, any()) } returns mockk()
    }

    @Test
    fun `GIVEN a new recent tab on bind THEN set the title text and load the tab icon`() {
        RecentTabViewHolder(binding.root, interactor, icons).bindTab(tab)

        assertEquals(tab.content.title, binding.recentTabTitle.text)

        verify { icons.loadIntoView(binding.recentTabIcon, IconRequest(tab.content.url)) }
    }

    @Test
    fun `WHEN a recent tab item is clicked THEN interactor is called`() {
        RecentTabViewHolder(binding.root, interactor, icons).bindTab(tab)

        binding.root.performClick()

        verify { interactor.onRecentTabClicked(tab.id) }
    }

    @Test
    fun `WHEN a recent tab icon exists THEN load it`() {
        val bitmap = testContext.getDrawable(R.drawable.ic_search)!!.toBitmap()
        val tabWithIcon = tab.copy(content = tab.content.copy(icon = bitmap))
        val viewHolder = RecentTabViewHolder(binding.root, interactor, icons)

        assertNull(binding.recentTabIcon.drawable)

        viewHolder.bindTab(tabWithIcon)

        assertNotNull(binding.recentTabIcon.drawable)
    }

    @Test
    fun `WHEN a recent tab does not have a title THEN show the url`() {
        val tabWithoutTitle = createTab(url = "https://mozilla.org")

        RecentTabViewHolder(binding.root, interactor, icons).bindTab(tabWithoutTitle)

        assertEquals(tabWithoutTitle.content.url, binding.recentTabTitle.text)
    }
}
