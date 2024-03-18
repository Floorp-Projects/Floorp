/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabhistory

import android.view.View
import io.mockk.CapturingSlot
import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.just
import io.mockk.mockk
import io.mockk.slot
import io.mockk.verify
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.IconRequest
import mozilla.components.ui.widgets.WidgetSiteItemView
import org.junit.Before
import org.junit.Test

class TabHistoryViewHolderTest {

    @MockK(relaxed = true)
    private lateinit var view: WidgetSiteItemView

    @MockK private lateinit var interactor: TabHistoryViewInteractor

    @MockK private lateinit var icons: BrowserIcons
    private lateinit var holder: TabHistoryViewHolder
    private lateinit var onClick: CapturingSlot<View.OnClickListener>

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
        onClick = slot()

        every { view.setOnClickListener(capture(onClick)) } just Runs
        every { icons.loadIntoView(view.iconView, any()) } returns mockk()

        holder = TabHistoryViewHolder(view, interactor, icons)
    }

    @Test
    fun `calls interactor on click`() {
        every { interactor.goToHistoryItem(any()) } just Runs

        val item = mockk<TabHistoryItem>(relaxed = true)
        holder.bind(item)
        onClick.captured.onClick(mockk())
        verify { interactor.goToHistoryItem(item) }
    }

    @Test
    fun `binds title and url`() {
        holder.bind(unselectedItem)

        verify { view.setText(label = "Firefox", caption = "https://firefox.com") }
        verify { icons.loadIntoView(view.iconView, IconRequest("https://firefox.com")) }
    }

    @Test
    fun `binds background`() {
        holder.bind(selectedItem)
        verify { view.setBackgroundColor(any()) }

        holder.bind(unselectedItem)
        verify { view.background = null }
    }
}
