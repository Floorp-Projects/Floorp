/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.content.Context
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.menu.view.MenuButton
import org.junit.Assert
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.utils.Settings

@ExperimentalCoroutinesApi
class HomeFragmentTest {

    private lateinit var settings: Settings
    private lateinit var context: Context
    private lateinit var homeFragment: HomeFragment

    @Before
    fun setup() {
        context = mockk(relaxed = true)
        settings = mockk(relaxed = true)

        homeFragment = spyk(HomeFragment())

        every { homeFragment.context } returns context
        every { context.settings() } returns settings
    }

    @Test
    fun `GIVEN showTopFrecentSites is false WHEN getTopSitesConfig is called THEN it returns TopSitesConfig with null frecencyConfig`() {
        every { settings.showTopFrecentSites } returns false
        every { settings.topSitesMaxLimit } returns 10

        val topSitesConfig = homeFragment.getTopSitesConfig()

        Assert.assertNull(topSitesConfig.frecencyConfig)
    }

    @Test
    fun `GIVEN showTopFrecentSites is true WHEN getTopSitesConfig is called THEN it returns TopSitesConfig with non-null frecencyConfig`() {
        every { context.settings().showTopFrecentSites } returns true
        every { settings.topSitesMaxLimit } returns 10

        val topSitesConfig = homeFragment.getTopSitesConfig()

        Assert.assertNotNull(topSitesConfig.frecencyConfig)
    }

    @Test
    fun `GIVEN a topSitesMaxLimit WHEN getTopSitesConfig is called THEN it returns TopSitesConfig with totalSites = topSitesMaxLimit`() {
        val topSitesMaxLimit = 10
        every { settings.topSitesMaxLimit } returns topSitesMaxLimit

        val topSitesConfig = homeFragment.getTopSitesConfig()

        Assert.assertEquals(topSitesMaxLimit, topSitesConfig.totalSites)
    }

    @Test
    fun `WHEN configuration changed menu is dismissed`() {
        val menuButton: MenuButton = mockk(relaxed = true)
        homeFragment.getMenuButton = { menuButton }
        homeFragment.onConfigurationChanged(mockk(relaxed = true))

        verify(exactly = 1) { menuButton.dismissMenu() }
    }
}
