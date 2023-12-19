/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.content.Context
import android.view.View
import androidx.coordinatorlayout.widget.CoordinatorLayout
import io.mockk.every
import io.mockk.mockk
import io.mockk.slot
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.state.state.SessionState
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.feature.contextmenu.ContextMenuCandidate
import mozilla.components.feature.session.behavior.EngineViewBrowserToolbarBehavior
import mozilla.components.ui.widgets.VerticalSwipeRefreshLayout
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.utils.Settings

class BaseBrowserFragmentTest {
    private lateinit var fragment: TestBaseBrowserFragment
    private lateinit var swipeRefreshLayout: VerticalSwipeRefreshLayout
    private lateinit var engineView: EngineView
    private lateinit var settings: Settings
    private lateinit var testContext: Context

    @Before
    fun setup() {
        fragment = spyk(TestBaseBrowserFragment())
        swipeRefreshLayout = mockk(relaxed = true)
        engineView = mockk(relaxed = true)
        settings = mockk(relaxed = true)
        testContext = mockk(relaxed = true)

        every { testContext.components.settings } returns settings
        every { fragment.isAdded } returns true
        every { fragment.activity } returns mockk()
        every { fragment.requireContext() } returns testContext
        every { fragment.getEngineView() } returns engineView
        every { fragment.getSwipeRefreshLayout() } returns swipeRefreshLayout
        every { swipeRefreshLayout.layoutParams } returns mockk<CoordinatorLayout.LayoutParams>(relaxed = true)
    }

    @Test
    fun `initializeEngineView should setDynamicToolbarMaxHeight to 0 if top toolbar is forced for a11y`() {
        every { settings.shouldUseBottomToolbar } returns false
        every { settings.shouldUseFixedTopToolbar } returns true

        fragment.initializeEngineView(13)

        verify { engineView.setDynamicToolbarMaxHeight(0) }
    }

    @Test
    fun `initializeEngineView should setDynamicToolbarMaxHeight to 0 if bottom toolbar is forced for a11y`() {
        every { settings.shouldUseBottomToolbar } returns true
        every { settings.shouldUseFixedTopToolbar } returns true

        fragment.initializeEngineView(13)

        verify { engineView.setDynamicToolbarMaxHeight(0) }
    }

    @Test
    fun `initializeEngineView should setDynamicToolbarMaxHeight to toolbar height if dynamic toolbar is enabled`() {
        every { settings.shouldUseFixedTopToolbar } returns false
        every { settings.isDynamicToolbarEnabled } returns true

        fragment.initializeEngineView(13)

        verify { engineView.setDynamicToolbarMaxHeight(13) }
    }

    @Test
    fun `initializeEngineView should setDynamicToolbarMaxHeight to 0 if dynamic toolbar is disabled`() {
        every { settings.shouldUseFixedTopToolbar } returns false
        every { settings.isDynamicToolbarEnabled } returns false

        fragment.initializeEngineView(13)

        verify { engineView.setDynamicToolbarMaxHeight(0) }
    }

    @Test
    fun `initializeEngineView should set EngineViewBrowserToolbarBehavior when dynamic toolbar is enabled`() {
        every { settings.shouldUseFixedTopToolbar } returns false
        every { settings.isDynamicToolbarEnabled } returns true
        val params: CoordinatorLayout.LayoutParams = mockk(relaxed = true)
        every { params.behavior } returns mockk(relaxed = true)
        every { swipeRefreshLayout.layoutParams } returns params
        val behavior = slot<EngineViewBrowserToolbarBehavior>()

        fragment.initializeEngineView(13)

        // EngineViewBrowserToolbarBehavior constructor parameters are not properties, we cannot check them.
        // Ensure just that the right behavior is set.
        verify { params.behavior = capture(behavior) }
    }

    @Test
    @Suppress("ktlint:standard:max-line-length")
    fun `initializeEngineView should set toolbar height as EngineView parent's bottom margin when using bottom toolbar`() {
        every { settings.isDynamicToolbarEnabled } returns false
        every { settings.shouldUseBottomToolbar } returns true

        fragment.initializeEngineView(13)

        verify { (swipeRefreshLayout.layoutParams as CoordinatorLayout.LayoutParams).bottomMargin = 13 }
    }

    @Test
    fun `initializeEngineView set toolbar height as EngineView parent's bottom margin if top toolbar`() {
        every { settings.shouldUseBottomToolbar } returns false
        every { settings.shouldUseFixedTopToolbar } returns true

        fragment.initializeEngineView(13)

        verify { (swipeRefreshLayout.layoutParams as CoordinatorLayout.LayoutParams).bottomMargin = 13 }
    }

    @Test
    @Suppress("MaxLineLength")
    fun `initializeEngineView set toolbar height as EngineView parent's bottom margin if bottom toolbar`() {
        every { settings.shouldUseBottomToolbar } returns true
        every { settings.shouldUseFixedTopToolbar } returns true

        fragment.initializeEngineView(13)

        verify { (swipeRefreshLayout.layoutParams as CoordinatorLayout.LayoutParams).bottomMargin = 13 }
    }
}

private class TestBaseBrowserFragment : BaseBrowserFragment() {
    override fun getContextMenuCandidates(
        context: Context,
        view: View,
    ): List<ContextMenuCandidate> {
        // no-op
        return emptyList()
    }

    override fun navToQuickSettingsSheet(tab: SessionState, sitePermissions: SitePermissions?) {
        // no-op
    }
}
