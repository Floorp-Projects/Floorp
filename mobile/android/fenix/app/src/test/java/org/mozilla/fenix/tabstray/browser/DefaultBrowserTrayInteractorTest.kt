/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.content.Context
import android.content.res.Resources
import android.util.DisplayMetrics
import androidx.recyclerview.widget.GridLayoutManager
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.spyk
import io.mockk.unmockkStatic
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.tabstray.TrayPagerAdapter
import org.mozilla.fenix.tabstray.ext.numberOfGridColumns
import org.mozilla.fenix.utils.Settings

class DefaultBrowserTrayInteractorTest {

    @Before
    fun setup() {
        mockkStatic("org.mozilla.fenix.tabstray.ext.ContextKt")
    }

    @After
    fun shutdown() {
        unmockkStatic("org.mozilla.fenix.tabstray.ext.ContextKt")
    }

    @Test
    fun `WHEN pager position is synced tabs THEN return a list layout manager`() {
        val interactor =
            DefaultBrowserTrayInteractor(mockk(), mockk(), mockk(), mockk(), mockk(), mockk())

        val result = interactor.getLayoutManagerForPosition(
            mockk(),
            TrayPagerAdapter.POSITION_SYNCED_TABS
        )

        assertEquals(1, (result as GridLayoutManager).spanCount)
    }

    @Test
    fun `WHEN setting is grid view THEN return grid layout manager`() {
        val context = mockk<Context>()
        val settings = mockk<Settings>()
        val interactor =
            DefaultBrowserTrayInteractor(mockk(), mockk(), mockk(), mockk(), settings, mockk())

        every { context.numberOfGridColumns }.answers { 4 }
        every { settings.gridTabView }.answers { true }

        val result = interactor.getLayoutManagerForPosition(
            context,
            TrayPagerAdapter.POSITION_NORMAL_TABS
        )

        assertEquals(4, (result as GridLayoutManager).spanCount)
    }

    @Test
    fun `WHEN setting is list view THEN return list layout manager`() {
        val context = mockk<Context>()
        val settings = mockk<Settings>()
        val interactor =
            DefaultBrowserTrayInteractor(mockk(), mockk(), mockk(), mockk(), settings, mockk())

        every { context.numberOfGridColumns }.answers { 4 }
        every { settings.gridTabView }.answers { false }

        val result = interactor.getLayoutManagerForPosition(
            context,
            TrayPagerAdapter.POSITION_NORMAL_TABS
        )

        // Should NOT be 4.
        assertEquals(1, (result as GridLayoutManager).spanCount)
    }

    @Test
    fun `WHEN screen density is very low THEN numberOfGridColumns will still be a minimum of 2`() {
        val context = mockk<Context>()
        val resources = mockk<Resources>()
        val displayMetrics = spyk<DisplayMetrics> {
            widthPixels = 1
            density = 1f
        }
        every { context.resources } returns resources
        every { resources.displayMetrics } returns displayMetrics

        val result = context.numberOfGridColumns

        assertEquals(2, result)
    }
}
