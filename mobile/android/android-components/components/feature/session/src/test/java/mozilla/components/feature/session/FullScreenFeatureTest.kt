/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.view.WindowManager
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

class FullScreenFeatureTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `Starting without tabs`() {
        var viewPort: Int? = null
        var fullscreen: Boolean? = null

        val store = BrowserStore()
        val feature = FullScreenFeature(
            store = store,
            sessionUseCases = mock(),
            tabId = null,
            viewportFitChanged = { value -> viewPort = value },
            fullScreenChanged = { value -> fullscreen = value },
        )

        feature.start()
        store.waitUntilIdle()

        assertNull(viewPort)
        assertNull(fullscreen)
    }

    @Test
    fun `Starting with selected tab will not invoke callbacks with default state`() {
        var viewPort: Int? = null
        var fullscreen: Boolean? = null

        val store = BrowserStore(
            BrowserState(
                tabs = listOf(createTab("https://www.mozilla.org", id = "A")),
                selectedTabId = "A",
            ),
        )

        val feature = FullScreenFeature(
            store = store,
            sessionUseCases = mock(),
            tabId = null,
            viewportFitChanged = { value -> viewPort = value },
            fullScreenChanged = { value -> fullscreen = value },
        )

        feature.start()
        store.waitUntilIdle()

        assertNull(viewPort)
        assertNull(fullscreen)
    }

    @Test
    fun `Starting with selected tab`() {
        var viewPort: Int? = null
        var fullscreen: Boolean? = null

        val store = BrowserStore(
            BrowserState(
                tabs = listOf(createTab("https://www.mozilla.org", id = "A")),
                selectedTabId = "A",
            ),
        )

        store.dispatch(
            ContentAction.FullScreenChangedAction(
                "A",
                true,
            ),
        ).joinBlocking()

        store.dispatch(
            ContentAction.ViewportFitChangedAction(
                "A",
                42,
            ),
        ).joinBlocking()

        val feature = FullScreenFeature(
            store = store,
            sessionUseCases = mock(),
            tabId = null,
            viewportFitChanged = { value -> viewPort = value },
            fullScreenChanged = { value -> fullscreen = value },
        )

        feature.start()
        store.waitUntilIdle()

        assertEquals(42, viewPort)
        assertTrue(fullscreen!!)
    }

    @Test
    fun `Selected tab switching to fullscreen mode`() {
        var viewPort: Int? = null
        var fullscreen: Boolean? = null

        val store = BrowserStore(
            BrowserState(
                tabs = listOf(createTab("https://www.mozilla.org", id = "A")),
                selectedTabId = "A",
            ),
        )

        val feature = FullScreenFeature(
            store = store,
            sessionUseCases = mock(),
            tabId = null,
            viewportFitChanged = { value -> viewPort = value },
            fullScreenChanged = { value -> fullscreen = value },
        )

        feature.start()

        store.dispatch(
            ContentAction.FullScreenChangedAction(
                "A",
                true,
            ),
        ).joinBlocking()

        assertNull(viewPort)
        assertTrue(fullscreen!!)
    }

    @Test
    fun `Selected tab changing viewport`() {
        var viewPort: Int? = null
        var fullscreen: Boolean? = null

        val store = BrowserStore(
            BrowserState(
                tabs = listOf(createTab("https://www.mozilla.org", id = "A")),
                selectedTabId = "A",
            ),
        )

        val feature = FullScreenFeature(
            store = store,
            sessionUseCases = mock(),
            tabId = null,
            viewportFitChanged = { value -> viewPort = value },
            fullScreenChanged = { value -> fullscreen = value },
        )

        feature.start()

        store.dispatch(
            ContentAction.FullScreenChangedAction(
                "A",
                true,
            ),
        ).joinBlocking()

        store.dispatch(
            ContentAction.ViewportFitChangedAction(
                "A",
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES,
            ),
        ).joinBlocking()

        assertNotEquals(0, viewPort)
        assertEquals(WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES, viewPort)
        assertTrue(fullscreen!!)
    }

    @Test
    fun `Fixed tab switching to fullscreen mode and back`() {
        var viewPort: Int? = null
        var fullscreen: Boolean? = null

        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "A"),
                    createTab("https://www.firefox.com", id = "B"),
                    createTab("https://getpocket.com", id = "C"),
                ),
                selectedTabId = "A",
            ),
        )

        val feature = FullScreenFeature(
            store = store,
            sessionUseCases = mock(),
            tabId = "B",
            viewportFitChanged = { value -> viewPort = value },
            fullScreenChanged = { value -> fullscreen = value },
        )

        feature.start()

        store.dispatch(
            ContentAction.FullScreenChangedAction(
                "B",
                true,
            ),
        ).joinBlocking()

        store.dispatch(
            ContentAction.ViewportFitChangedAction(
                "B",
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES,
            ),
        ).joinBlocking()

        assertEquals(WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES, viewPort)
        assertTrue(fullscreen!!)

        store.dispatch(
            ContentAction.FullScreenChangedAction(
                "B",
                false,
            ),
        ).joinBlocking()

        store.dispatch(
            ContentAction.ViewportFitChangedAction(
                "B",
                0,
            ),
        ).joinBlocking()

        assertEquals(0, viewPort)
        assertFalse(fullscreen!!)
    }

    @Test
    fun `Callback functions no longer get invoked when stopped, but get new value on next start`() {
        var viewPort: Int? = null
        var fullscreen: Boolean? = null

        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "A"),
                    createTab("https://www.firefox.com", id = "B"),
                    createTab("https://getpocket.com", id = "C"),
                ),
                selectedTabId = "A",
            ),
        )

        val feature = FullScreenFeature(
            store = store,
            sessionUseCases = mock(),
            tabId = "B",
            viewportFitChanged = { value -> viewPort = value },
            fullScreenChanged = { value -> fullscreen = value },
        )

        store.dispatch(
            ContentAction.FullScreenChangedAction(
                "B",
                true,
            ),
        ).joinBlocking()

        store.dispatch(
            ContentAction.ViewportFitChangedAction(
                "B",
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER,
            ),
        ).joinBlocking()

        feature.start()

        assertEquals(WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER, viewPort)
        assertTrue(fullscreen!!)

        feature.stop()

        store.dispatch(
            ContentAction.FullScreenChangedAction(
                "B",
                false,
            ),
        ).joinBlocking()

        store.dispatch(
            ContentAction.ViewportFitChangedAction(
                "B",
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES,
            ),
        ).joinBlocking()

        assertEquals(WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER, viewPort)
        assertTrue(fullscreen!!)

        feature.start()

        assertEquals(WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES, viewPort)
        assertFalse(fullscreen!!)
    }

    @Test
    fun `onBackPressed will invoke usecase for active fullscreen mode`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "A"),
                    createTab("https://www.firefox.com", id = "B"),
                    createTab("https://getpocket.com", id = "C"),
                ),
                selectedTabId = "A",
            ),
        )

        val exitUseCase: SessionUseCases.ExitFullScreenUseCase = mock()
        val useCases: SessionUseCases = mock()
        doReturn(exitUseCase).`when`(useCases).exitFullscreen

        val feature = FullScreenFeature(
            store = store,
            sessionUseCases = useCases,
            tabId = "B",
            fullScreenChanged = {},
        )

        feature.start()

        store.dispatch(
            ContentAction.FullScreenChangedAction(
                "B",
                true,
            ),
        ).joinBlocking()

        store.dispatch(
            ContentAction.ViewportFitChangedAction(
                "B",
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES,
            ),
        ).joinBlocking()

        assertTrue(feature.onBackPressed())

        verify(exitUseCase).invoke("B")
    }

    @Test
    fun `Fullscreen tab gets removed`() {
        var viewPort: Int? = null
        var fullscreen: Boolean? = null

        val store = BrowserStore(
            BrowserState(
                tabs = listOf(createTab("https://www.mozilla.org", id = "A")),
                selectedTabId = "A",
            ),
        )

        val feature = FullScreenFeature(
            store = store,
            sessionUseCases = mock(),
            tabId = null,
            viewportFitChanged = { value -> viewPort = value },
            fullScreenChanged = { value -> fullscreen = value },
        )

        feature.start()

        store.dispatch(
            ContentAction.FullScreenChangedAction(
                "A",
                true,
            ),
        ).joinBlocking()

        store.dispatch(
            ContentAction.ViewportFitChangedAction(
                "A",
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES,
            ),
        ).joinBlocking()

        assertEquals(WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES, viewPort)
        assertTrue(fullscreen!!)

        store.dispatch(
            TabListAction.RemoveTabAction(tabId = "A"),
        ).joinBlocking()

        assertEquals(0, viewPort)
        assertFalse(fullscreen!!)
    }

    @Test
    fun `onBackPressed will not invoke usecase if not in fullscreen mode`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "A"),
                    createTab("https://www.firefox.com", id = "B"),
                    createTab("https://getpocket.com", id = "C"),
                ),
                selectedTabId = "A",
            ),
        )

        val exitUseCase: SessionUseCases.ExitFullScreenUseCase = mock()
        val useCases: SessionUseCases = mock()
        doReturn(exitUseCase).`when`(useCases).exitFullscreen

        val feature = FullScreenFeature(
            store = store,
            sessionUseCases = useCases,
            fullScreenChanged = {},
        )

        feature.start()

        assertFalse(feature.onBackPressed())

        verify(exitUseCase, never()).invoke("B")
    }

    @Test
    fun `onBackPressed getting invoked without any tabs to observe`() {
        val exitUseCase: SessionUseCases.ExitFullScreenUseCase = mock()
        val useCases: SessionUseCases = mock()
        doReturn(exitUseCase).`when`(useCases).exitFullscreen

        val feature = FullScreenFeature(
            store = BrowserStore(),
            sessionUseCases = useCases,
            fullScreenChanged = {},
        )

        // Invoking onBackPressed without fullscreen mode
        assertFalse(feature.onBackPressed())

        verify(exitUseCase, never()).invoke(ArgumentMatchers.anyString())
    }
}
