/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs

import android.app.PendingIntent
import android.graphics.Bitmap
import android.graphics.Color
import android.view.ViewGroup
import android.view.Window
import android.widget.FrameLayout
import android.widget.ImageButton
import androidx.core.view.forEach
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.test.UnconfinedTestDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.CustomTabActionButtonConfig
import mozilla.components.browser.state.state.CustomTabConfig
import mozilla.components.browser.state.state.CustomTabMenuItem
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.tabs.CustomTabsUseCases
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyList
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`

@RunWith(AndroidJUnit4::class)
class CustomTabsToolbarFeatureTest {
    @Test
    fun `start without sessionId invokes nothing`() {
        val store = BrowserStore()
        val toolbar: BrowserToolbar = mock()
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(CustomTabsToolbarFeature(store, toolbar, sessionId = null, useCases = useCases) {})

        feature.start()

        verify(feature, never()).init(any())
    }

    @Test
    fun `start calls initialize with the sessionId`() {
        val tab = createCustomTab("https://www.mozilla.org", id = "mozilla")

        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = BrowserToolbar(testContext)
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases) {})

        feature.start()

        verify(feature).init(tab)

        // Calling start again should NOT call init again

        feature.start()

        verify(feature, times(1)).init(tab)
    }

    @Test
    fun `initialize updates toolbar`() {
        val tab = createCustomTab("https://www.mozilla.org", id = "mozilla")

        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = BrowserToolbar(testContext)
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases) {}

        feature.init(tab)

        assertFalse(toolbar.display.onUrlClicked.invoke())
    }

    @Test
    fun `initialize updates toolbar, window and text color`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                toolbarColor = Color.RED,
                navigationBarColor = Color.BLUE,
            ),
        )

        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val window: Window = mock()
        `when`(window.decorView).thenReturn(mock())
        val feature = CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases, window = window) {}

        feature.init(tab)

        verify(toolbar).setBackgroundColor(Color.RED)
        verify(window).statusBarColor = Color.RED
        verify(window).navigationBarColor = Color.BLUE

        assertEquals(Color.WHITE, toolbar.display.colors.title)
        assertEquals(Color.WHITE, toolbar.display.colors.text)
    }

    @Test
    fun `initialize does not update toolbar background if flag is set`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                toolbarColor = Color.RED,
            ),
        )

        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val window: Window = mock()
        `when`(window.decorView).thenReturn(mock())

        run {
            val feature = CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                window = window,
                updateToolbarBackground = false,
            ) {}

            feature.init(tab)

            verify(toolbar, never()).setBackgroundColor(Color.RED)
        }

        run {
            val feature = CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                window = window,
                updateToolbarBackground = true,
            ) {}

            feature.init(tab)

            verify(toolbar).setBackgroundColor(Color.RED)
        }
    }

    @Test
    fun `adds close button`() {
        val tab = createCustomTab("https://www.mozilla.org", id = "mozilla", config = CustomTabConfig())

        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases) {}

        feature.start()

        verify(toolbar).addNavigationAction(any())
    }

    @Test
    fun `doesn't add close button if the button should be hidden`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                showCloseButton = false,
            ),
        )

        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases) {}

        feature.start()

        verify(toolbar, never()).addNavigationAction(any())
    }

    @Test
    fun `close button invokes callback and removes session`() {
        val middleware = CaptureActionsMiddleware<BrowserState, BrowserAction>()

        val store = BrowserStore(
            middleware = listOf(middleware),
            initialState = BrowserState(
                customTabs = listOf(
                    createCustomTab("https://www.mozilla.org", id = "mozilla", config = CustomTabConfig()),
                ),
            ),
        )

        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        var closeClicked = false
        val feature = CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases) {
            closeClicked = true
        }

        feature.start()

        verify(toolbar).addNavigationAction(any())

        val button = extractActionView(toolbar, testContext.getString(R.string.mozac_feature_customtabs_exit_button))

        middleware.assertNotDispatched(CustomTabListAction.RemoveCustomTabAction::class)

        button?.performClick()

        assertTrue(closeClicked)

        middleware.assertLastAction(CustomTabListAction.RemoveCustomTabAction::class) { action ->
            assertEquals("mozilla", action.tabId)
        }
    }

    @Test
    fun `does not add share button by default`() {
        val tab = createCustomTab("https://www.mozilla.org", id = "mozilla", config = CustomTabConfig())
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases) {})

        feature.start()

        verify(feature, never()).addShareButton(tab)
        verify(toolbar, never()).addBrowserAction(any())
    }

    @Test
    fun `adds share button`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                showShareMenuItem = true,
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases) {})

        feature.start()

        verify(feature).addShareButton(tab)
        verify(toolbar).addBrowserAction(any())
    }

    @Test
    fun `share button uses custom share listener`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                showShareMenuItem = true,
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        var clicked = false
        val feature = CustomTabsToolbarFeature(
            store,
            toolbar,
            sessionId = "mozilla",
            useCases = useCases,
            shareListener = { clicked = true },
        ) {}

        feature.start()

        val captor = argumentCaptor<Toolbar.ActionButton>()
        verify(toolbar).addBrowserAction(captor.capture())

        val button = captor.value.createView(FrameLayout(testContext))
        button.performClick()
        assertTrue(clicked)
    }

    @Test
    fun `initialize calls addActionButton`() {
        val tab = createCustomTab("https://www.mozilla.org", id = "mozilla", config = CustomTabConfig())
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases) {})

        feature.start()

        verify(feature).addActionButton(any(), any())
    }

    @Test
    fun `action button is scaled to 24 width and 24 height`() {
        val captor = argumentCaptor<Toolbar.ActionButton>()
        val size = 48
        val pendingIntent: PendingIntent = mock()
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                actionButtonConfig = CustomTabActionButtonConfig(
                    description = "Button",
                    icon = Bitmap.createBitmap(IntArray(size * size), size, size, Bitmap.Config.ARGB_8888),
                    pendingIntent = pendingIntent,
                ),
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases) {})

        feature.start()

        verify(feature).addActionButton(any(), any())
        verify(toolbar).addBrowserAction(captor.capture())

        val button = captor.value.createView(FrameLayout(testContext))
        assertEquals(24, (button as ImageButton).drawable.intrinsicHeight)
        assertEquals(24, button.drawable.intrinsicWidth)
    }

    @Test
    fun `initialize calls addMenuItems when config has items`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                menuItems = listOf(
                    CustomTabMenuItem("Share", mock()),
                ),
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(CustomTabsToolbarFeature(store, toolbar, sessionId = "mozilla", useCases = useCases) {})

        feature.start()

        verify(feature).addMenuItems(any(), anyList(), anyInt())
    }

    @Test
    fun `initialize calls addMenuItems when menuBuilder has items`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                menuItems = listOf(
                    CustomTabMenuItem("Share", mock()),
                ),
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                menuBuilder = BrowserMenuBuilder(listOf(mock(), mock())),
            ) {},
        )

        feature.start()

        verify(feature).addMenuItems(any(), anyList(), anyInt())
    }

    @Test
    fun `menu items added WITHOUT current items`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                menuItems = listOf(
                    CustomTabMenuItem("Share", mock()),
                ),
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
            ) {},
        )

        feature.start()

        val menuBuilder = toolbar.display.menuBuilder
        assertEquals(1, menuBuilder!!.items.size)
    }

    @Test
    fun `menu items added WITH current items`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                menuItems = listOf(
                    CustomTabMenuItem("Share", mock()),
                ),
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                menuBuilder = BrowserMenuBuilder(listOf(mock(), mock())),
            ) {},
        )

        feature.start()

        val menuBuilder = toolbar.display.menuBuilder
        assertEquals(3, menuBuilder!!.items.size)
    }

    @Test
    fun `menu item added at specified index`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                menuItems = listOf(
                    CustomTabMenuItem("Share", mock()),
                ),
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                menuBuilder = BrowserMenuBuilder(listOf(mock(), mock())),
                menuItemIndex = 1,
            ) {},
        )

        feature.start()

        val menuBuilder = toolbar.display.menuBuilder!!

        assertEquals(3, menuBuilder.items.size)
        assertTrue(menuBuilder.items[1] is SimpleBrowserMenuItem)
    }

    @Test
    fun `menu item added appended if index too large`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                menuItems = listOf(
                    CustomTabMenuItem("Share", mock()),
                ),
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                menuBuilder = BrowserMenuBuilder(listOf(mock(), mock())),
                menuItemIndex = 4,
            ) {},
        )

        feature.start()

        val menuBuilder = toolbar.display.menuBuilder!!

        assertEquals(3, menuBuilder.items.size)
        assertTrue(menuBuilder.items[2] is SimpleBrowserMenuItem)
    }

    @Test
    fun `menu item added appended if index too small`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                menuItems = listOf(
                    CustomTabMenuItem("Share", mock()),
                ),
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                menuBuilder = BrowserMenuBuilder(listOf(mock(), mock())),
                menuItemIndex = -4,
            ) {},
        )

        feature.start()

        val menuBuilder = toolbar.display.menuBuilder!!

        assertEquals(3, menuBuilder.items.size)
        assertTrue(menuBuilder.items[0] is SimpleBrowserMenuItem)
    }

    @Test
    fun `onBackPressed removes initialized session`() {
        val store = BrowserStore(
            initialState = BrowserState(
                customTabs = listOf(
                    createCustomTab("https://www.mozilla.org", id = "mozilla", config = CustomTabConfig()),
                ),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        var closeExecuted = false
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                menuBuilder = BrowserMenuBuilder(listOf(mock(), mock())),
                menuItemIndex = 4,
            ) {
                closeExecuted = true
            },
        )

        feature.start()

        val result = feature.onBackPressed()

        assertTrue(result)
        assertTrue(closeExecuted)
    }

    @Test
    fun `onBackPressed without a session does nothing`() {
        val tab = createCustomTab("https://www.mozilla.org", id = "mozilla", config = CustomTabConfig())
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        var closeExecuted = false
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = null,
                useCases = useCases,
                menuBuilder = BrowserMenuBuilder(listOf(mock(), mock())),
                menuItemIndex = 4,
            ) {
                closeExecuted = true
            },
        )

        feature.start()

        val result = feature.onBackPressed()

        assertFalse(result)
        assertFalse(closeExecuted)
    }

    @Test
    fun `onBackPressed with uninitialized feature returns false`() {
        val tab = createCustomTab("https://www.mozilla.org", id = "mozilla", config = CustomTabConfig())
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        var closeExecuted = false
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = null,
                useCases = useCases,
                menuBuilder = BrowserMenuBuilder(listOf(mock(), mock())),
                menuItemIndex = 4,
            ) {
                closeExecuted = true
            },
        )

        val result = feature.onBackPressed()

        assertFalse(result)
        assertFalse(closeExecuted)
    }

    @Test
    fun `readableColor - White on Black`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                toolbarColor = Color.BLACK,
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                menuBuilder = BrowserMenuBuilder(listOf(mock(), mock())),
                menuItemIndex = 4,
            ) {},
        )

        feature.start()

        assertEquals(Color.WHITE, feature.readableColor)
        assertEquals(Color.WHITE, toolbar.display.colors.text)
    }

    @Test
    fun `readableColor - Black on White`() {
        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                toolbarColor = Color.WHITE,
            ),
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                menuBuilder = BrowserMenuBuilder(listOf(mock(), mock())),
                menuItemIndex = 4,
            ) {},
        )

        feature.start()

        assertEquals(Color.BLACK, feature.readableColor)
        assertEquals(Color.BLACK, toolbar.display.colors.text)
    }

    @Test
    fun `show title only if not empty`() {
        val dispatcher = UnconfinedTestDispatcher()
        Dispatchers.setMain(dispatcher)

        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(),
            title = "",
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                menuBuilder = BrowserMenuBuilder(listOf(mock(), mock())),
                menuItemIndex = 4,
            ) {},
        )

        feature.start()

        assertEquals("", toolbar.title)

        store.dispatch(
            ContentAction.UpdateTitleAction(
                "mozilla",
                "Internet for people, not profit - Mozilla",
            ),
        ).joinBlocking()

        assertEquals("Internet for people, not profit - Mozilla", toolbar.title)

        Dispatchers.resetMain()
    }

    @Test
    fun `Will use URL as title if title was shown once and is now empty`() {
        val dispatcher = UnconfinedTestDispatcher()
        Dispatchers.setMain(dispatcher)

        val tab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(),
            title = "",
        )
        val store = BrowserStore(
            BrowserState(
                customTabs = listOf(tab),
            ),
        )
        val toolbar = spy(BrowserToolbar(testContext))
        val useCases = CustomTabsUseCases(
            store = store,
            loadUrlUseCase = SessionUseCases(store).loadUrl,
        )
        val feature = spy(
            CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = "mozilla",
                useCases = useCases,
                menuBuilder = BrowserMenuBuilder(listOf(mock(), mock())),
                menuItemIndex = 4,
            ) {},
        )

        feature.start()

        feature.start()

        assertEquals("", toolbar.title)

        store.dispatch(
            ContentAction.UpdateUrlAction("mozilla", "https://www.mozilla.org/en-US/firefox/"),
        ).joinBlocking()

        assertEquals("", toolbar.title)

        store.dispatch(
            ContentAction.UpdateTitleAction("mozilla", "Firefox - Protect your life online with privacy-first products"),
        ).joinBlocking()

        assertEquals("Firefox - Protect your life online with privacy-first products", toolbar.title)

        store.dispatch(
            ContentAction.UpdateUrlAction("mozilla", "https://github.com/mozilla-mobile/android-components"),
        ).joinBlocking()

        assertEquals("https://github.com/mozilla-mobile/android-components", toolbar.title)

        store.dispatch(
            ContentAction.UpdateTitleAction("mozilla", "Le GitHub"),
        ).joinBlocking()

        assertEquals("Le GitHub", toolbar.title)

        store.dispatch(
            ContentAction.UpdateUrlAction("mozilla", "https://github.com/mozilla-mobile/fenix"),
        ).joinBlocking()

        assertEquals("https://github.com/mozilla-mobile/fenix", toolbar.title)

        store.dispatch(
            ContentAction.UpdateTitleAction("mozilla", ""),
        ).joinBlocking()

        assertEquals("https://github.com/mozilla-mobile/fenix", toolbar.title)

        store.dispatch(
            ContentAction.UpdateTitleAction("mozilla", "A collection of Android libraries to build browsers or browser-like applications."),
        ).joinBlocking()

        assertEquals("A collection of Android libraries to build browsers or browser-like applications.", toolbar.title)

        store.dispatch(
            ContentAction.UpdateTitleAction("mozilla", ""),
        ).joinBlocking()

        assertEquals("https://github.com/mozilla-mobile/fenix", toolbar.title)
    }

    private fun extractActionView(
        browserToolbar: BrowserToolbar,
        contentDescription: String,
    ): ImageButton? {
        var actionView: ImageButton? = null

        browserToolbar.forEach { group ->
            val viewGroup = group as ViewGroup

            viewGroup.forEach inner@{ subGroup ->
                if (subGroup is ViewGroup) {
                    subGroup.forEach {
                        if (it is ImageButton && it.contentDescription == contentDescription) {
                            actionView = it
                            return@inner
                        }
                    }
                }
            }
        }

        return actionView
    }
}
