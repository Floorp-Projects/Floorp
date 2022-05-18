/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.content.Context
import android.content.res.Configuration
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.view.isVisible
import androidx.lifecycle.LifecycleCoroutineScope
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.lifecycleScope
import androidx.navigation.NavController
import androidx.navigation.fragment.findNavController
import androidx.viewbinding.ViewBindings
import androidx.viewpager2.widget.ViewPager2
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.tabs.TabLayout
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.spyk
import io.mockk.unmockkStatic
import io.mockk.verify
import kotlinx.coroutines.CoroutineScope
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertSame
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.TabsTray
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.ComponentTabstray2Binding
import org.mozilla.fenix.databinding.ComponentTabstrayFabBinding
import org.mozilla.fenix.databinding.FragmentTabTrayDialogBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.helpers.MockkRetryTestRule
import org.mozilla.fenix.home.HomeScreenViewModel
import org.mozilla.fenix.tabstray.browser.BrowserTrayInteractor
import org.mozilla.fenix.tabstray.browser.InactiveTabsInteractor
import org.mozilla.fenix.tabstray.ext.showWithTheme
import org.mozilla.fenix.utils.allowUndo

@RunWith(FenixRobolectricTestRunner::class)
class TabsTrayFragmentTest {
    private lateinit var context: Context
    private lateinit var view: ViewGroup
    private lateinit var fragment: TabsTrayFragment
    private lateinit var tabsTrayBinding: ComponentTabstray2Binding
    private lateinit var tabsTrayDialogBinding: FragmentTabTrayDialogBinding
    private lateinit var fabButtonBinding: ComponentTabstrayFabBinding

    @get:Rule
    val mockkRule = MockkRetryTestRule()

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @Before
    fun setup() {
        context = mockk(relaxed = true)
        view = mockk(relaxed = true)
        val inflater = LayoutInflater.from(testContext)
        tabsTrayDialogBinding = FragmentTabTrayDialogBinding.inflate(inflater)
        tabsTrayBinding = ComponentTabstray2Binding.inflate(inflater)
        fabButtonBinding = ComponentTabstrayFabBinding.inflate(inflater)

        fragment = spyk(TabsTrayFragment())
        fragment._tabsTrayBinding = tabsTrayBinding
        fragment._tabsTrayDialogBinding = tabsTrayDialogBinding
        fragment._fabButtonBinding = fabButtonBinding
        every { fragment.context } returns context
        every { fragment.context } returns context
        every { fragment.viewLifecycleOwner } returns mockk(relaxed = true)
    }

    @Test
    fun `WHEN showUndoSnackbarForTab is called for a private tab with new tab button visible THEN an appropriate snackbar is shown`() {
        try {
            mockkStatic("org.mozilla.fenix.utils.UndoKt")
            mockkStatic("androidx.lifecycle.LifecycleOwnerKt")
            val lifecycleScope: LifecycleCoroutineScope = mockk(relaxed = true)
            every { any<LifecycleOwner>().lifecycleScope } returns lifecycleScope
            fabButtonBinding.newTabButton.isVisible = true
            every { fragment.context } returns testContext // needed for getString()
            every { any<CoroutineScope>().allowUndo(any(), any(), any(), any(), any(), any(), any(), any()) } just Runs
            every { fragment.requireView() } returns view

            fragment.showUndoSnackbarForTab(true)

            verify {
                lifecycleScope.allowUndo(
                    view,
                    testContext.getString(R.string.snackbar_private_tab_closed),
                    testContext.getString(R.string.snackbar_deleted_undo),
                    any(),
                    any(),
                    fabButtonBinding.newTabButton,
                    TabsTrayFragment.ELEVATION,
                    false
                )
            }
        } finally {
            unmockkStatic("org.mozilla.fenix.utils.UndoKt")
            unmockkStatic("androidx.lifecycle.LifecycleOwnerKt")
        }
    }

    @Test
    fun `WHEN showUndoSnackbarForTab is called for a private tab with new tab button not visible THEN an appropriate snackbar is shown`() {
        try {
            mockkStatic("org.mozilla.fenix.utils.UndoKt")
            mockkStatic("androidx.lifecycle.LifecycleOwnerKt")
            val lifecycleScope: LifecycleCoroutineScope = mockk(relaxed = true)
            every { any<LifecycleOwner>().lifecycleScope } returns lifecycleScope
            every { fragment.context } returns testContext // needed for getString()
            every { any<CoroutineScope>().allowUndo(any(), any(), any(), any(), any(), any(), any(), any()) } just Runs
            every { fragment.requireView() } returns view

            fragment.showUndoSnackbarForTab(true)

            verify {
                lifecycleScope.allowUndo(
                    view,
                    testContext.getString(R.string.snackbar_private_tab_closed),
                    testContext.getString(R.string.snackbar_deleted_undo),
                    any(),
                    any(),
                    null,
                    TabsTrayFragment.ELEVATION,
                    false
                )
            }
        } finally {
            unmockkStatic("org.mozilla.fenix.utils.UndoKt")
            unmockkStatic("androidx.lifecycle.LifecycleOwnerKt")
        }
    }

    @Test
    fun `WHEN showUndoSnackbarForTab is called for a normal tab with new tab button visible THEN an appropriate snackbar is shown`() {
        try {
            mockkStatic("org.mozilla.fenix.utils.UndoKt")
            mockkStatic("androidx.lifecycle.LifecycleOwnerKt")
            val lifecycleScope: LifecycleCoroutineScope = mockk(relaxed = true)
            every { any<LifecycleOwner>().lifecycleScope } returns lifecycleScope
            fabButtonBinding.newTabButton.isVisible = true
            every { fragment.context } returns testContext // needed for getString()
            every { any<CoroutineScope>().allowUndo(any(), any(), any(), any(), any(), any(), any(), any()) } just Runs
            every { fragment.requireView() } returns view

            fragment.showUndoSnackbarForTab(false)

            verify {
                lifecycleScope.allowUndo(
                    view,
                    testContext.getString(R.string.snackbar_tab_closed),
                    testContext.getString(R.string.snackbar_deleted_undo),
                    any(),
                    any(),
                    fabButtonBinding.newTabButton,
                    TabsTrayFragment.ELEVATION,
                    false
                )
            }
        } finally {
            unmockkStatic("org.mozilla.fenix.utils.UndoKt")
            unmockkStatic("androidx.lifecycle.LifecycleOwnerKt")
        }
    }

    @Test
    fun `WHEN showUndoSnackbarForTab is called for a normal tab with new tab button not visible THEN an appropriate snackbar is shown`() {
        try {
            mockkStatic("org.mozilla.fenix.utils.UndoKt")
            mockkStatic("androidx.lifecycle.LifecycleOwnerKt")
            val lifecycleScope: LifecycleCoroutineScope = mockk(relaxed = true)
            every { any<LifecycleOwner>().lifecycleScope } returns lifecycleScope
            every { fragment.context } returns testContext // needed for getString()
            every { any<CoroutineScope>().allowUndo(any(), any(), any(), any(), any(), any(), any(), any()) } just Runs
            every { fragment.requireView() } returns view

            fragment.showUndoSnackbarForTab(false)

            verify {
                lifecycleScope.allowUndo(
                    view,
                    testContext.getString(R.string.snackbar_tab_closed),
                    testContext.getString(R.string.snackbar_deleted_undo),
                    any(),
                    any(),
                    null,
                    TabsTrayFragment.ELEVATION,
                    false
                )
            }
        } finally {
            unmockkStatic("org.mozilla.fenix.utils.UndoKt")
            unmockkStatic("androidx.lifecycle.LifecycleOwnerKt")
        }
    }

    @Test
    fun `WHEN setupPager is called THEN it sets the tray adapter and disables user initiated scrolling`() {
        val store: TabsTrayStore = mockk()
        val lifecycleOwner = mockk<LifecycleOwner>(relaxed = true)
        val trayInteractor: TabsTrayInteractor = mockk()
        val browserInteractor: BrowserTrayInteractor = mockk()
        val navigationInteractor: NavigationInteractor = mockk()
        val inactiveTabsInteractor: InactiveTabsInteractor = mockk()
        val browserStore: BrowserStore = mockk()
        every { context.components.core.store } returns browserStore

        fragment.setupPager(
            context = context,
            lifecycleOwner = lifecycleOwner,
            store = store,
            trayInteractor = trayInteractor,
            browserInteractor = browserInteractor,
            navigationInteractor = navigationInteractor,
            inactiveTabsInteractor = inactiveTabsInteractor,
        )

        val adapter = (tabsTrayBinding.tabsTray.adapter as TrayPagerAdapter)
        assertSame(context, adapter.context)
        assertSame(lifecycleOwner, adapter.lifecycleOwner)
        assertSame(store, adapter.tabsTrayStore)
        assertSame(trayInteractor, adapter.tabsTrayInteractor)
        assertSame(browserInteractor, adapter.browserInteractor)
        assertSame(navigationInteractor, adapter.navInteractor)
        assertSame(browserStore, adapter.browserStore)
        assertFalse(tabsTrayBinding.tabsTray.isUserInputEnabled)
    }

    @Test
    fun `WHEN setupMenu is called THEN it sets a 3 dot menu click listener to open the tray menu`() {
        try {
            mockkStatic("org.mozilla.fenix.tabstray.ext.BrowserMenuKt")
            val navigationInteractor: NavigationInteractor = mockk()
            every { context.components.core.store } returns mockk()
            every { fragment.tabsTrayStore } returns mockk()
            val menu: BrowserMenu = mockk {
                every { showWithTheme(any()) } just Runs
            }
            val menuBuilder: MenuIntegration = mockk(relaxed = true) {
                every { build() } returns menu
            }
            every { fragment.getTrayMenu(any(), any(), any(), any(), any()) } returns menuBuilder

            assertNull(TabsTray.menuOpened.testGetValue())

            fragment.setupMenu(navigationInteractor)
            tabsTrayBinding.tabTrayOverflow.performClick()

            assertNotNull(TabsTray.menuOpened.testGetValue())
            verify { menuBuilder.build() }
            verify { menu.showWithTheme(tabsTrayBinding.tabTrayOverflow) }
        } finally {
            unmockkStatic("org.mozilla.fenix.tabstray.ext.BrowserMenuKt")
        }
    }

    @Test
    fun `WHEN getTrayMenu is called THEN it returns a MenuIntegration initialized with the passed in parameters`() {
        val browserStore: BrowserStore = mockk()
        val tabsTrayStore: TabsTrayStore = mockk()
        val tabLayout: TabLayout = mockk()
        val navigationInteractor: NavigationInteractor = mockk()

        val result = fragment.getTrayMenu(context, browserStore, tabsTrayStore, tabLayout, navigationInteractor)

        assertSame(context, result.context)
        assertSame(browserStore, result.browserStore)
        assertSame(tabsTrayStore, result.tabsTrayStore)
        assertSame(tabLayout, result.tabLayout)
        assertSame(navigationInteractor, result.navigationInteractor)
    }

    @Test
    fun `WHEN setupBackgroundDismissalListener is called THEN it sets a click listener for tray's tabLayout and handle`() {
        var clickCount = 0
        val callback: (View) -> Unit = { clickCount++ }

        fragment.setupBackgroundDismissalListener(callback)

        tabsTrayDialogBinding.tabLayout.performClick()
        assertEquals(1, clickCount)
        tabsTrayBinding.handle.performClick()
        assertEquals(2, clickCount)
    }

    @Test
    fun `WHEN dismissTabsTrayAndNavigateHome is called with a sessionId THEN it navigates to home to delete that sessions and dismisses the tray`() {
        every { fragment.navigateToHomeAndDeleteSession(any()) } just Runs
        every { fragment.dismissTabsTray() } just Runs

        fragment.dismissTabsTrayAndNavigateHome("test")

        verify { fragment.navigateToHomeAndDeleteSession("test") }
        verify { fragment.dismissTabsTray() }
    }

    @Test
    fun `WHEN navigateToHomeAndDeleteSession is called with a sessionId THEN it navigates to home and transmits there the sessionId`() {
        try {
            mockkStatic("androidx.fragment.app.FragmentViewModelLazyKt")
            mockkStatic("androidx.navigation.fragment.FragmentKt")
            mockkStatic("org.mozilla.fenix.ext.NavControllerKt")
            val viewModel: HomeScreenViewModel = mockk(relaxed = true)
            every { fragment.homeViewModel } returns viewModel
            val navController: NavController = mockk(relaxed = true)
            every { fragment.findNavController() } returns navController

            fragment.navigateToHomeAndDeleteSession("test")

            verify { viewModel.sessionToDelete = "test" }
            verify { navController.navigate(NavGraphDirections.actionGlobalHome()) }
        } finally {
            unmockkStatic("org.mozilla.fenix.ext.NavControllerKt")
            unmockkStatic("androidx.navigation.fragment.FragmentKt")
            unmockkStatic("androidx.fragment.app.FragmentViewModelLazyKt")
        }
    }

    @Test
    fun `WHEN selectTabPosition is called with a position and smooth scroll indication THEN it scrolls to that tab and selects it`() {
        val tabsTray: ViewPager2 = mockk(relaxed = true)
        val tab: TabLayout.Tab = mockk(relaxed = true)
        val tabLayout: TabLayout = mockk {
            every { getTabAt(any()) } returns tab
        }

        mockkStatic(ViewBindings::class) {
            every { ViewBindings.findChildViewById<View>(tabsTrayBinding.root, tabsTrayBinding.tabsTray.id) } returns tabsTray
            every { ViewBindings.findChildViewById<View>(tabsTrayBinding.root, tabsTrayBinding.tabLayout.id) } returns tabLayout

            tabsTrayBinding = ComponentTabstray2Binding.bind(tabsTrayBinding.root)
            fragment._tabsTrayBinding = tabsTrayBinding

            fragment.selectTabPosition(2, true)

            verify { tabsTray.setCurrentItem(2, true) }
            verify { tab.select() }
        }
    }

    @Test
    fun `WHEN dismissTabsTray is called THEN it dismisses the tray`() {
        every { fragment.dismissAllowingStateLoss() } just Runs

        fragment.dismissTabsTray()

        verify { fragment.dismissAllowingStateLoss() }
    }

    @Test
    fun `WHEN onConfigurationChanged is called THEN it delegates the tray behavior manager to update the tray`() {
        val trayBehaviorManager: TabSheetBehaviorManager = mockk(relaxed = true)
        fragment.trayBehaviorManager = trayBehaviorManager
        val newConfiguration = Configuration()
        every { context.settings().gridTabView } returns false

        fragment.onConfigurationChanged(newConfiguration)

        verify { trayBehaviorManager.updateDependingOnOrientation(newConfiguration.orientation) }
    }

    @Test
    fun `WHEN the tabs tray is declared in XML THEN certain options are set for the behavior`() {
        tabsTrayBinding = ComponentTabstray2Binding.inflate(
            LayoutInflater.from(testContext), CoordinatorLayout(testContext), true
        )
        val behavior = BottomSheetBehavior.from(tabsTrayBinding.tabWrapper)

        assertFalse(behavior.isFitToContents)
        assertFalse(behavior.skipCollapsed)
        assert(behavior.halfExpandedRatio <= 0.001f)
    }

    @Test
    fun `GIVEN a grid TabView WHEN onConfigurationChanged is called THEN the adapter structure is updated`() {
        every { context.settings().gridTabView } returns true
        val adapter = mockk<TrayPagerAdapter>(relaxed = true)
        tabsTrayBinding.tabsTray.adapter = adapter
        fragment._tabsTrayBinding = tabsTrayBinding
        val trayBehaviorManager: TabSheetBehaviorManager = mockk(relaxed = true)
        fragment.trayBehaviorManager = trayBehaviorManager
        val newConfiguration = Configuration()

        fragment.onConfigurationChanged(newConfiguration)

        verify { adapter.notifyDataSetChanged() }
    }

    @Test
    fun `GIVEN a list TabView WHEN onConfigurationChanged is called THEN the adapter structure is NOT updated`() {
        every { context.settings().gridTabView } returns false
        val adapter = mockk<TrayPagerAdapter>(relaxed = true)
        tabsTrayBinding.tabsTray.adapter = adapter
        fragment._tabsTrayBinding = tabsTrayBinding
        val trayBehaviorManager: TabSheetBehaviorManager = mockk(relaxed = true)
        fragment.trayBehaviorManager = trayBehaviorManager
        val newConfiguration = Configuration()

        fragment.onConfigurationChanged(newConfiguration)

        verify(exactly = 0) { adapter.notifyDataSetChanged() }
    }
}
