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
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.state.CustomTabActionButtonConfig
import mozilla.components.browser.state.state.CustomTabConfig
import mozilla.components.browser.state.state.CustomTabMenuItem
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyList
import org.mockito.Mockito.`when`
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.anyString
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class CustomTabsToolbarFeatureTest {

    @Test
    fun `start without sessionId invokes nothing`() {
        val sessionManager: SessionManager = spy(SessionManager(mock()))
        val session: Session = mock()

        val feature = spy(CustomTabsToolbarFeature(sessionManager, mock()) {})

        feature.start()

        verify(sessionManager, never()).findSessionById(anyString())
        verify(feature, never()).initialize(session)
    }

    @Test
    fun `start calls initialize with the sessionId`() {
        val sessionManager: SessionManager = mock()
        val session: Session = mock()
        val toolbar = BrowserToolbar(testContext)
        val feature = spy(CustomTabsToolbarFeature(sessionManager, toolbar, "") {})

        `when`(sessionManager.findSessionById(anyString())).thenReturn(session)
        `when`(session.customTabConfig).thenReturn(mock())
        doNothing().`when`(feature).addCloseButton(session, null)

        feature.start()

        verify(feature).initialize(session)

        // Calling start again should NOT call init again

        feature.start()

        verify(feature, times(1)).initialize(session)
    }

    @Test
    fun `stop calls unregister`() {
        val sessionManager: SessionManager = mock()
        val session: Session = mock()
        val toolbar = BrowserToolbar(testContext)

        `when`(sessionManager.findSessionById(anyString())).thenReturn(session)

        val feature = CustomTabsToolbarFeature(sessionManager, toolbar, "") {}
        feature.stop()

        verify(session).unregister(any())
    }

    @Test
    fun `initialize returns true if session is a customtab`() {
        val session: Session = mock()
        val toolbar = spy(BrowserToolbar(testContext))
        val feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "") {})

        var initialized = feature.initialize(session)

        assertFalse(initialized)

        `when`(session.customTabConfig).thenReturn(mock())

        initialized = feature.initialize(session)

        assertTrue(initialized)
    }

    @Test
    fun `initialize updates toolbar`() {
        val session: Session = mock()
        val toolbar = spy(BrowserToolbar(testContext))
        `when`(session.customTabConfig).thenReturn(mock())

        val feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "") {})

        feature.initialize(session)

        assertFalse(toolbar.display.onUrlClicked.invoke())
    }

    @Test
    fun `initialize calls updateToolbarColor`() {
        val sessionManager: SessionManager = mock()
        val session: Session = mock()
        val toolbar = spy(BrowserToolbar(testContext))
        `when`(session.customTabConfig).thenReturn(mock())

        val feature = spy(CustomTabsToolbarFeature(sessionManager, toolbar, "") {})

        feature.initialize(session)

        verify(feature).updateToolbarColor(anyInt(), anyInt())
    }

    @Test
    fun `updateToolbarColor changes background and textColor`() {
        val session: Session = mock()
        val toolbar = spy(BrowserToolbar(testContext))
        `when`(session.customTabConfig).thenReturn(mock())

        val feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "") {})

        val colors = toolbar.display.colors

        feature.updateToolbarColor(null, null)

        assertEquals(colors, toolbar.display.colors)
        assertNotEquals(Color.WHITE, toolbar.display.colors.title)
        assertNotEquals(Color.WHITE, toolbar.display.colors.text)

        feature.updateToolbarColor(123, 456)

        assertEquals(Color.WHITE, toolbar.display.colors.title)
        assertEquals(Color.WHITE, toolbar.display.colors.text)
    }

    @Test
    fun `updateToolbarColor changes status bar color`() {
        val session: Session = mock()
        val toolbar: BrowserToolbar = BrowserToolbar(testContext)
        val window: Window = mock()
        `when`(session.customTabConfig).thenReturn(mock())
        `when`(window.decorView).thenReturn(mock())

        val feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "", window = window) {})

        feature.updateToolbarColor(null, null)

        verify(window, never()).statusBarColor = anyInt()
        verify(window, never()).navigationBarColor = anyInt()

        feature.updateToolbarColor(123, 456)

        verify(window).statusBarColor = 123
        verify(window).navigationBarColor = 456
    }

    @Test
    fun `initialize calls addCloseButton`() {
        val session: Session = mock()
        val toolbar = BrowserToolbar(testContext)
        `when`(session.customTabConfig).thenReturn(CustomTabConfig())

        val feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "") {})

        feature.initialize(session)

        verify(feature).addCloseButton(session, null)
    }

    @Test
    fun `initialize doesn't call addCloseButton if the button should be hidden`() {
        val session: Session = mock()
        val toolbar = BrowserToolbar(testContext)
        `when`(session.customTabConfig).thenReturn(CustomTabConfig(showCloseButton = false))

        val feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "") {})

        feature.initialize(session)

        verify(feature, never()).addCloseButton(session, null)
    }

    @Test
    fun `close button invokes callback and removes session`() {
        val session: Session = mock()
        val sessionManager: SessionManager = mock()
        val toolbar = BrowserToolbar(testContext)
        var closeClicked = false
        val feature = spy(CustomTabsToolbarFeature(sessionManager, toolbar, "") { closeClicked = true })

        `when`(session.customTabConfig).thenReturn(CustomTabConfig())

        feature.initialize(session)

        val button = extractActionView(toolbar, testContext.getString(R.string.mozac_feature_customtabs_exit_button))
        button?.performClick()

        assertTrue(closeClicked)
        verify(sessionManager).remove(session)
    }

    @Test
    fun `initialize calls addShareButton`() {
        val session: Session = mock()
        val toolbar = BrowserToolbar(testContext)
        val config: CustomTabConfig = mock()
        `when`(session.customTabConfig).thenReturn(config)
        `when`(session.url).thenReturn("https://mozilla.org")

        val feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "") {})

        feature.initialize(session)

        verify(feature, never()).addShareButton(session)

        // Show share menu only if config.showShareMenuItem has true
        `when`(config.showShareMenuItem).thenReturn(true)

        feature.initialize(session)

        verify(feature).addShareButton(session)
    }

    @Test
    fun `share button uses custom share listener`() {
        val session: Session = mock()
        val toolbar = spy(BrowserToolbar(testContext))
        val captor = argumentCaptor<Toolbar.ActionButton>()
        var clicked = false
        val feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "", shareListener = { clicked = true }) {})

        `when`(session.customTabConfig).thenReturn(mock())

        feature.addShareButton(session)

        verify(toolbar).addBrowserAction(captor.capture())

        val button = captor.value.createView(FrameLayout(testContext))
        button.performClick()
        assertTrue(clicked)
    }

    @Test
    fun `initialize calls addActionButton`() {
        val session: Session = mock()
        val toolbar = BrowserToolbar(testContext)
        `when`(session.customTabConfig).thenReturn(mock())

        val feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "") {})

        feature.initialize(session)

        verify(feature).addActionButton(any(), any())
    }

    @Test
    fun `add action button is invoked`() {
        val session: Session = mock()
        val toolbar = spy(BrowserToolbar(testContext))
        val captor = argumentCaptor<Toolbar.ActionButton>()
        val feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "") {})
        val customTabConfig: CustomTabConfig = mock()
        val actionConfig: CustomTabActionButtonConfig = mock()
        val size = 24
        val closeButtonIcon = Bitmap.createBitmap(IntArray(size * size), size, size, Bitmap.Config.ARGB_8888)
        val pendingIntent: PendingIntent = mock()

        `when`(session.customTabConfig).thenReturn(customTabConfig)
        `when`(session.url).thenReturn("https://example.com")

        feature.addActionButton(session, null)

        verify(toolbar, never()).addBrowserAction(any())

        // Show action button only when CustomTabActionButtonConfig is not null
        `when`(customTabConfig.actionButtonConfig).thenReturn(actionConfig)
        `when`(actionConfig.description).thenReturn("test desc")
        `when`(actionConfig.pendingIntent).thenReturn(pendingIntent)
        `when`(actionConfig.icon).thenReturn(closeButtonIcon)

        feature.addActionButton(session, actionConfig)

        verify(toolbar).addBrowserAction(captor.capture())

        val button = captor.value.createView(FrameLayout(testContext))
        button.performClick()
        verify(pendingIntent).send(any(), anyInt(), any())
    }

    @Test
    fun `initialize calls addMenuItems when config has items`() {
        val session: Session = mock()
        val toolbar = BrowserToolbar(testContext)
        val customTabConfig: CustomTabConfig = mock()
        `when`(session.customTabConfig).thenReturn(customTabConfig)
        `when`(customTabConfig.menuItems).thenReturn(emptyList())

        val feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "") {})

        feature.initialize(session)

        verify(feature, never()).addMenuItems(any(), anyList(), anyInt())

        `when`(customTabConfig.menuItems).thenReturn(listOf(CustomTabMenuItem("Share", mock())))

        feature.initialize(session)

        verify(feature).addMenuItems(any(), anyList(), anyInt())
    }

    @Test
    fun `initialize calls addMenuItems when menuBuilder has items`() {
        val session: Session = mock()
        val menuBuilder: BrowserMenuBuilder = mock()
        val toolbar = BrowserToolbar(testContext)
        val customTabConfig: CustomTabConfig = mock()
        `when`(session.customTabConfig).thenReturn(customTabConfig)
        `when`(customTabConfig.menuItems).thenReturn(emptyList())
        `when`(menuBuilder.items).thenReturn(listOf(mock(), mock()))

        val feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "", menuBuilder) {})

        feature.initialize(session)

        verify(feature).addMenuItems(any(), anyList(), anyInt())
    }

    @Test
    fun `initialize never calls addMenuItems when no config or builder items available`() {
        val session: Session = mock()
        val menuBuilder: BrowserMenuBuilder = mock()
        val toolbar = BrowserToolbar(testContext)
        val customTabConfig: CustomTabConfig = mock()
        `when`(session.customTabConfig).thenReturn(customTabConfig)

        // With NO config or builder.
        var feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "") {})

        feature.initialize(session)

        verify(feature, never()).addMenuItems(any(), anyList(), anyInt())

        // With only builder but NO items.
        feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "", menuBuilder) {})

        feature.initialize(session)

        // With only builder and items.
        `when`(menuBuilder.items).thenReturn(listOf())

        feature.initialize(session)

        verify(feature, never()).addMenuItems(any(), anyList(), anyInt())

        // With config with NO items and builder with items.
        `when`(customTabConfig.menuItems).thenReturn(emptyList())

        feature.initialize(session)

        verify(feature, never()).addMenuItems(any(), anyList(), anyInt())
    }

    @Test
    fun `menu items added WITHOUT current items`() {
        val session: Session = mock()
        val toolbar = spy(BrowserToolbar(testContext))
        val feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "") {})
        val customTabConfig: CustomTabConfig = mock()

        `when`(session.customTabConfig).thenReturn(customTabConfig)
        `when`(customTabConfig.menuItems).thenReturn(emptyList())

        feature.addMenuItems(session, listOf(CustomTabMenuItem("Share", mock())), 0)

        val menuBuilder = toolbar.display.menuBuilder
        assertEquals(1, menuBuilder!!.items.size)
    }

    @Test
    fun `menu items added WITH current items`() {
        val session: Session = mock()
        val toolbar = spy(BrowserToolbar(testContext))
        val builder: BrowserMenuBuilder = mock()
        val feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "", builder) {})
        val customTabConfig: CustomTabConfig = mock()
        `when`(session.customTabConfig).thenReturn(customTabConfig)
        `when`(customTabConfig.menuItems).thenReturn(emptyList())
        `when`(builder.items).thenReturn(listOf(mock(), mock()))

        feature.addMenuItems(session, listOf(CustomTabMenuItem("Share", mock())), 0)

        val menuBuilder = toolbar.display.menuBuilder!!
        assertEquals(3, menuBuilder.items.size)
    }

    @Test
    fun `menu item added at specified index`() {
        val session: Session = mock()
        val toolbar = spy(BrowserToolbar(testContext))
        val builder: BrowserMenuBuilder = mock()
        val feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "", builder) {})
        val customTabConfig: CustomTabConfig = mock()
        `when`(session.customTabConfig).thenReturn(customTabConfig)
        `when`(customTabConfig.menuItems).thenReturn(emptyList())
        `when`(builder.items).thenReturn(listOf(mock(), mock()))

        feature.addMenuItems(session, listOf(CustomTabMenuItem("Share", mock())), 1)

        val menuBuilder = toolbar.display.menuBuilder!!

        assertEquals(3, menuBuilder.items.size)
        assertTrue(menuBuilder.items[1] is SimpleBrowserMenuItem)
    }

    @Test
    fun `menu item added appended if index too large`() {
        val session: Session = mock()
        val toolbar = spy(BrowserToolbar(testContext))
        val builder: BrowserMenuBuilder = mock()
        val feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "", builder) {})
        val customTabConfig: CustomTabConfig = mock()
        `when`(session.customTabConfig).thenReturn(customTabConfig)
        `when`(customTabConfig.menuItems).thenReturn(emptyList())
        `when`(builder.items).thenReturn(listOf(mock(), mock()))

        feature.addMenuItems(session, listOf(CustomTabMenuItem("Share", mock())), 4)

        val menuBuilder = toolbar.display.menuBuilder!!

        assertEquals(3, menuBuilder.items.size)
        assertTrue(menuBuilder.items[2] is SimpleBrowserMenuItem)
    }

    @Test
    fun `menu item added appended if index too small`() {
        val session: Session = mock()
        val toolbar = spy(BrowserToolbar(testContext))
        val builder: BrowserMenuBuilder = mock()
        val feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "", builder) {})
        val customTabConfig: CustomTabConfig = mock()
        `when`(session.customTabConfig).thenReturn(customTabConfig)
        `when`(customTabConfig.menuItems).thenReturn(emptyList())
        `when`(builder.items).thenReturn(listOf(mock(), mock()))

        feature.addMenuItems(session, listOf(CustomTabMenuItem("Share", mock())), -4)

        val menuBuilder = toolbar.display.menuBuilder!!

        assertEquals(3, menuBuilder.items.size)
        assertTrue(menuBuilder.items[2] is SimpleBrowserMenuItem)
    }

    @Test
    fun `onBackPressed removes initialized session`() {
        val sessionId = "123"
        val session: Session = mock()
        val toolbar = BrowserToolbar(testContext)
        val sessionManager: SessionManager = mock()
        var closeExecuted = false
        `when`(session.customTabConfig).thenReturn(mock())
        `when`(sessionManager.findSessionById(anyString())).thenReturn(session)

        val feature = spy(CustomTabsToolbarFeature(sessionManager, toolbar, sessionId) { closeExecuted = true })
        feature.initialized = true

        val result = feature.onBackPressed()

        assertTrue(result)
        assertTrue(closeExecuted)
    }

    @Test
    fun `onBackPressed without a session does nothing`() {
        val sessionId = null
        val session: Session = mock()
        val toolbar = BrowserToolbar(testContext)
        val sessionManager: SessionManager = mock()
        var closeExecuted = false
        `when`(session.customTabConfig).thenReturn(mock())
        `when`(sessionManager.findSessionById(anyString())).thenReturn(session)

        val feature = spy(CustomTabsToolbarFeature(sessionManager, toolbar, sessionId) { closeExecuted = true })

        val result = feature.onBackPressed()

        assertFalse(result)
        assertFalse(closeExecuted)
    }

    @Test
    fun `onBackPressed with uninitialized feature returns false`() {
        val sessionId = "123"
        val session: Session = mock()
        val toolbar = BrowserToolbar(testContext)
        val sessionManager: SessionManager = mock()
        var closeExecuted = false
        `when`(sessionManager.findSessionById(anyString())).thenReturn(session)

        val feature = spy(CustomTabsToolbarFeature(sessionManager, toolbar, sessionId) {
            closeExecuted = true
        })
        feature.initialized = false

        val result = feature.onBackPressed()

        assertFalse(result)
        assertFalse(closeExecuted)
    }

    @Test
    fun `keep readableColor if toolbarColor is provided`() {
        val sessionManager: SessionManager = mock()
        val toolbar = BrowserToolbar(testContext)
        val session: Session = mock()
        val customTabConfig: CustomTabConfig = mock()
        val feature = spy(CustomTabsToolbarFeature(sessionManager, toolbar, "") {})

        assertEquals(Color.WHITE, feature.readableColor)

        `when`(sessionManager.findSessionById(anyString())).thenReturn(session)
        `when`(session.customTabConfig).thenReturn(customTabConfig)

        feature.initialize(session)

        assertEquals(Color.WHITE, feature.readableColor)

        `when`(customTabConfig.toolbarColor).thenReturn(Color.WHITE)

        feature.initialize(session)

        assertEquals(Color.BLACK, feature.readableColor)
    }

    @Test
    fun `show title only if not empty`() {
        val sessionManager: SessionManager = mock()
        val toolbar = BrowserToolbar(testContext)
        val session = spy(Session("https://mozilla.org"))
        val customTabConfig: CustomTabConfig = mock()
        val feature = spy(CustomTabsToolbarFeature(sessionManager, toolbar, "") {})
        val title = "Internet for people, not profit - Mozilla"

        `when`(sessionManager.findSessionById(anyString())).thenReturn(session)
        `when`(session.customTabConfig).thenReturn(customTabConfig)

        feature.start()

        session.notifyObservers { onTitleChanged(session, "") }

        assertEquals("", toolbar.title)

        session.notifyObservers { onTitleChanged(session, title) }

        assertEquals(title, toolbar.title)
    }

    @Test
    fun `Will use URL as title if title was shown once and is now empty`() {
        val sessionManager = SessionManager(mock())
        val toolbar = BrowserToolbar(testContext)
        val session = Session("https://mozilla.org").also {
            it.customTabConfig = mock()
            sessionManager.add(it)
        }
        val feature = spy(CustomTabsToolbarFeature(sessionManager, toolbar, session.id) {})

        feature.start()

        assertEquals("", toolbar.title)

        session.url = "https://www.mozilla.org/en-US/firefox/"

        assertEquals("", toolbar.title)

        session.title = "Firefox - Protect your life online with privacy-first products"

        assertEquals("Firefox - Protect your life online with privacy-first products", toolbar.title)

        session.url = "https://github.com/mozilla-mobile/android-components"

        assertEquals("Firefox - Protect your life online with privacy-first products", toolbar.title)

        session.title = ""

        assertEquals("https://github.com/mozilla-mobile/android-components", toolbar.title)

        session.title = "A collection of Android libraries to build browsers or browser-like applications."

        assertEquals("A collection of Android libraries to build browsers or browser-like applications.", session.title)

        session.title = ""

        assertEquals("https://github.com/mozilla-mobile/android-components", toolbar.title)
    }

    private fun extractActionView(
        browserToolbar: BrowserToolbar,
        contentDescription: String
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
