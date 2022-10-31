/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import android.os.Looper.getMainLooper
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.isActive
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.test.mock
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.robolectric.Shadows.shadowOf

@RunWith(AndroidJUnit4::class)
class ToolbarBehaviorControllerTest {

    @Test
    fun `Controller should check the status of the provided custom tab id`() {
        val customTabContent: ContentState = mock()
        val normalTabContent: ContentState = mock()
        val state = spy(
            BrowserState(
                tabs = listOf(TabSessionState("123", normalTabContent)),
                customTabs = listOf(CustomTabSessionState("ct", customTabContent, config = mock())),
                selectedTabId = "123",
            ),
        )
        val store = BrowserStore(state)
        val controller = ToolbarBehaviorController(mock(), store, "ct")

        assertNull(controller.updatesScope)

        controller.start()
        shadowOf(getMainLooper()).idle()

        assertNotNull(controller.updatesScope)
        verify(customTabContent, times(3)).loading
        verify(normalTabContent, never()).loading
    }

    @Test
    fun `Controller should check the status of the currently selected tab if not initialized with a custom tab id`() {
        val customTabContent: ContentState = mock()
        val normalTabContent: ContentState = mock()
        val state = spy(
            BrowserState(
                tabs = listOf(TabSessionState("123", normalTabContent)),
                customTabs = listOf(CustomTabSessionState("ct", customTabContent, config = mock())),
                selectedTabId = "123",
            ),
        )
        val store = BrowserStore(state)
        val controller = ToolbarBehaviorController(mock(), store)

        assertNull(controller.updatesScope)

        controller.start()
        shadowOf(getMainLooper()).idle()

        assertNotNull(controller.updatesScope)
        verify(customTabContent, never()).loading
        verify(normalTabContent, times(3)).loading
    }

    @Test
    fun `Controller should disableScrolling if the current tab is loading`() {
        val normalTabContent = ContentState("url", loading = true)
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(TabSessionState("123", normalTabContent)),
                selectedTabId = "123",
            ),
        )
        val controller = spy(ToolbarBehaviorController(mock(), store))

        controller.start()
        shadowOf(getMainLooper()).idle()

        verify(controller).disableScrolling()
    }

    @Test
    fun `Controller should enableScrolling if the current tab is not loading`() {
        val normalTabContent = ContentState("url", loading = false)
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(TabSessionState("123", normalTabContent)),
                selectedTabId = "123",
            ),
        )
        val controller = spy(ToolbarBehaviorController(mock(), store))

        controller.start()
        shadowOf(getMainLooper()).idle()

        verify(controller).enableScrolling()
    }

    @Test
    fun `Controller should listening for tab updates if stop is called`() {
        val controller = spy(ToolbarBehaviorController(mock(), BrowserStore(BrowserState())))

        controller.start()
        shadowOf(getMainLooper()).idle()
        assertTrue(controller.updatesScope!!.isActive)

        controller.stop()
        assertFalse(controller.updatesScope!!.isActive)
    }

    @Test
    fun `Controller should disable toolbar scrolling when disableScrolling is called`() {
        val toolbar: Toolbar = mock()
        val controller = spy(ToolbarBehaviorController(toolbar, mock()))

        controller.disableScrolling()

        verify(toolbar).disableScrolling()
    }

    @Test
    fun `Controller should enable toolbar scrolling when enableScrolling is called`() {
        val toolbar: Toolbar = mock()
        val controller = spy(ToolbarBehaviorController(toolbar, mock()))

        controller.enableScrolling()

        verify(toolbar).enableScrolling()
    }

    @Test
    fun `Controller should expand the toolbar and set showToolbarAsExpanded to false when showToolbarAsExpanded is true`() {
        val normalTabContent = ContentState("url", showToolbarAsExpanded = true)
        val store = spy(
            BrowserStore(
                BrowserState(
                    tabs = listOf(TabSessionState("123", normalTabContent)),
                    selectedTabId = "123",
                ),
            ),
        )
        val controller = spy(ToolbarBehaviorController(mock(), store))

        controller.start()
        shadowOf(getMainLooper()).idle()

        verify(controller).expandToolbar()
        verify(store).dispatch(ContentAction.UpdateExpandedToolbarStateAction("123", false))
    }

    @Test
    fun `Controller should not expand the toolbar and not update the current state if showToolbarAsExpanded is false`() {
        val normalTabContent = ContentState("url", showToolbarAsExpanded = false)
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(TabSessionState("123", normalTabContent)),
                selectedTabId = "123",
            ),
        )
        val controller = spy(ToolbarBehaviorController(mock(), store))

        controller.start()
        shadowOf(getMainLooper()).idle()

        verify(controller, never()).expandToolbar()
    }

    @Test
    fun `GIVEN the current tab is loading an url WHEN the page is scrolled THEN expand toolbar`() {
        val tabContent = ContentState("loading", loading = true)
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(TabSessionState("tab_1", tabContent)),
                selectedTabId = "tab_1",
            ),
        )
        val controller = spy(ToolbarBehaviorController(mock(), store))

        controller.start()
        shadowOf(getMainLooper()).idle()

        verify(controller).expandToolbar()
    }
}
