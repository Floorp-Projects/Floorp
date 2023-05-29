/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.view.View.GONE
import android.view.View.VISIBLE
import android.view.ViewGroup
import androidx.coordinatorlayout.widget.CoordinatorLayout
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.tabstray.TabsTrayInfoBannerBinding.Companion.TAB_COUNT_SHOW_CFR
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
@OptIn(ExperimentalCoroutinesApi::class)
class TabsTrayInfoBannerBindingTest {

    private lateinit var store: BrowserStore
    private lateinit var view: ViewGroup
    private lateinit var interactor: NavigationInteractor
    private lateinit var settings: Settings

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Before
    fun setUp() {
        store = BrowserStore()
        view = CoordinatorLayout(testContext)
        interactor = mockk(relaxed = true)
        settings = Settings(testContext)

        every { testContext.components.settings } returns settings
    }

    @Test
    fun `WHEN tab number reaches CFR count THEN banner is shown`() {
        view.visibility = GONE

        val binding =
            TabsTrayInfoBannerBinding(
                context = testContext,
                store = store,
                infoBannerView = view,
                settings = settings,
                navigationInteractor = interactor,
            )

        binding.start()
        for (i in 1 until TAB_COUNT_SHOW_CFR) {
            store.dispatch(TabListAction.AddTabAction(createTab("https://mozilla.org")))
            store.waitUntilIdle()

            assert(view.visibility == GONE)
        }

        store.dispatch(TabListAction.AddTabAction(createTab("https://mozilla.org")))
        store.waitUntilIdle()
        assert(view.visibility == VISIBLE)
    }

    @Test
    fun `WHEN dismiss THEN auto close tabs info banner will not open tab settings`() {
        view.visibility = GONE
        settings.gridTabView = true

        val binding =
            TabsTrayInfoBannerBinding(
                context = testContext,
                store = store,
                infoBannerView = view,
                settings = settings,
                navigationInteractor = interactor,
            )

        binding.start()
        for (i in 1..TAB_COUNT_SHOW_CFR) {
            store.dispatch(TabListAction.AddTabAction(createTab("https://mozilla.org")))
            store.waitUntilIdle()
        }

        assert(view.visibility == VISIBLE)
        binding.banner?.dismissAction?.invoke()

        verify(exactly = 0) { interactor.onTabSettingsClicked() }
        assert(!settings.shouldShowAutoCloseTabsBanner)
    }
}
