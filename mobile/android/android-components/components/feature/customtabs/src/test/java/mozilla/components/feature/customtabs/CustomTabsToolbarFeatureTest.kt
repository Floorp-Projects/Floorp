/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.customtabs

import android.app.PendingIntent
import android.graphics.Bitmap
import android.graphics.Color
import android.widget.FrameLayout
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.tab.CustomTabActionButtonConfig
import mozilla.components.browser.session.tab.CustomTabConfig
import mozilla.components.browser.session.tab.CustomTabMenuItem
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
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
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
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
        val feature = spy(CustomTabsToolbarFeature(sessionManager, mock(), "") {})

        `when`(sessionManager.findSessionById(anyString())).thenReturn(session)
        `when`(session.customTabConfig).thenReturn(mock())
        doNothing().`when`(feature).addCloseButton(null)

        feature.start()

        verify(feature).initialize(session)

        // Calling start again should NOT call init again

        feature.start()

        verify(feature, times(1)).initialize(session)
    }

    @Test
    fun `initialize returns true if session is a customtab`() {
        val session: Session = mock()
        val toolbar = spy(BrowserToolbar(RuntimeEnvironment.application))
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
        val toolbar = spy(BrowserToolbar(RuntimeEnvironment.application))
        `when`(session.customTabConfig).thenReturn(mock())

        val feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "") {})

        feature.initialize(session)

        assertFalse(toolbar.onUrlClicked.invoke())
    }

    @Test
    fun `initialize calls updateToolbarColor`() {
        val sessionManager: SessionManager = mock()
        val session: Session = mock()
        val toolbar = spy(BrowserToolbar(RuntimeEnvironment.application))
        `when`(session.customTabConfig).thenReturn(mock())

        val feature = spy(CustomTabsToolbarFeature(sessionManager, toolbar, "") {})

        feature.initialize(session)

        verify(feature).updateToolbarColor(anyInt())
    }

    @Test
    fun `updateToolbarColor changes background and textColor`() {
        val session: Session = mock()
        val toolbar: BrowserToolbar = mock()
        `when`(session.customTabConfig).thenReturn(mock())

        val feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "") {})

        feature.updateToolbarColor(null)

        verify(toolbar, never()).setBackgroundColor(anyInt())
        verify(toolbar, never()).textColor = anyInt()

        feature.updateToolbarColor(123)

        verify(toolbar).setBackgroundColor(anyInt())
        verify(toolbar).textColor = anyInt()
    }

    @Test
    fun getReadableTextColor() {
        // White text color for a black background
        val white = CustomTabsToolbarFeature.getReadableTextColor(0)
        assertEquals(Color.WHITE, white)

        // Black text color for a white background
        val black = CustomTabsToolbarFeature.getReadableTextColor(0xFFFFFF)
        assertEquals(Color.BLACK, black)
    }

    @Test
    fun `initialize calls addCloseButton`() {
        val session: Session = mock()
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)
        `when`(session.customTabConfig).thenReturn(mock())

        val feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "") {})

        feature.initialize(session)

        verify(feature).addCloseButton(null)
    }

    @Test
    fun `initialize calls addShareButton`() {
        val session: Session = mock()
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)
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
        val toolbar = spy(BrowserToolbar(RuntimeEnvironment.application))
        val captor = argumentCaptor<Toolbar.ActionButton>()
        var clicked = false
        val feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "", shareListener = { clicked = true }) {})

        `when`(session.customTabConfig).thenReturn(mock())

        feature.addShareButton(session)

        verify(toolbar).addBrowserAction(captor.capture())

        val button = captor.value.createView(FrameLayout(RuntimeEnvironment.application))
        button.performClick()
        assertTrue(clicked)
    }

    @Test
    fun `initialize calls addActionButton`() {
        val session: Session = mock()
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)
        `when`(session.customTabConfig).thenReturn(mock())

        val feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "") {})

        feature.initialize(session)

        verify(feature).addActionButton(null)
    }

    @Test
    fun `add action button is invoked`() {
        val session: Session = mock()
        val toolbar = spy(BrowserToolbar(RuntimeEnvironment.application))
        val captor = argumentCaptor<Toolbar.ActionButton>()
        val feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "") {})
        val customTabConfig: CustomTabConfig = mock()
        val actionConfig: CustomTabActionButtonConfig = mock()
        val size = 24
        val closeButtonIcon = Bitmap.createBitmap(IntArray(size * size), size, size, Bitmap.Config.ARGB_8888)
        val pendingIntent: PendingIntent = mock()

        `when`(session.customTabConfig).thenReturn(customTabConfig)

        feature.addActionButton(null)

        verify(toolbar, never()).addBrowserAction(any())

        // Show action button only when CustomTabActionButtonConfig is not null
        `when`(customTabConfig.actionButtonConfig).thenReturn(actionConfig)
        `when`(actionConfig.description).thenReturn("test desc")
        `when`(actionConfig.pendingIntent).thenReturn(pendingIntent)
        `when`(actionConfig.icon).thenReturn(closeButtonIcon)

        feature.addActionButton(actionConfig)

        verify(toolbar).addBrowserAction(captor.capture())

        val button = captor.value.createView(FrameLayout(RuntimeEnvironment.application))
        button.performClick()
        verify(pendingIntent).send()
    }

    @Test
    fun `initialize calls addMenuItems`() {
        val session: Session = mock()
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)
        val customTabConfig: CustomTabConfig = mock()
        `when`(session.customTabConfig).thenReturn(customTabConfig)
        `when`(customTabConfig.menuItems).thenReturn(emptyList())

        val feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "") {})

        feature.initialize(session)

        verify(feature, never()).addMenuItems(anyList())

        `when`(customTabConfig.menuItems).thenReturn(listOf(CustomTabMenuItem("Share", mock())))

        feature.initialize(session)

        verify(feature).addMenuItems(anyList())
    }

    @Test
    fun `menu items added WITHOUT current items`() {
        val session: Session = mock()
        val toolbar = spy(BrowserToolbar(RuntimeEnvironment.application))
        val feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "") {})
        val captor = argumentCaptor<BrowserMenuBuilder>()
        val customTabConfig: CustomTabConfig = mock()

        `when`(session.customTabConfig).thenReturn(customTabConfig)
        `when`(customTabConfig.menuItems).thenReturn(emptyList())

        feature.addMenuItems(listOf(CustomTabMenuItem("Share", mock())))

        verify(toolbar).setMenuBuilder(captor.capture())
        assertEquals(1, captor.value.items.size)
    }

    @Test
    fun `menu items added WITH current items`() {
        val session: Session = mock()
        val toolbar = spy(BrowserToolbar(RuntimeEnvironment.application))
        val builder: BrowserMenuBuilder = mock()
        val feature = spy(CustomTabsToolbarFeature(mock(), toolbar, "", builder) {})
        val captor = argumentCaptor<BrowserMenuBuilder>()
        val customTabConfig: CustomTabConfig = mock()
        `when`(session.customTabConfig).thenReturn(customTabConfig)
        `when`(customTabConfig.menuItems).thenReturn(emptyList())
        `when`(builder.items).thenReturn(listOf(mock(), mock()))

        feature.addMenuItems(listOf(CustomTabMenuItem("Share", mock())))

        verify(toolbar).setMenuBuilder(captor.capture())

        assertEquals(3, captor.value.items.size)
    }

    @Test
    fun `onBackPressed removes session`() {
        val sessionId = "123"
        val session: Session = mock()
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)
        val sessionManager: SessionManager = mock()
        var closeExecuted = false
        `when`(session.customTabConfig).thenReturn(mock())
        `when`(sessionManager.findSessionById(anyString())).thenReturn(session)

        val feature = spy(CustomTabsToolbarFeature(sessionManager, toolbar, sessionId) { closeExecuted = true })
        val result = feature.onBackPressed()

        assertTrue(result)
        assertTrue(closeExecuted)
    }

    @Test
    fun `onBackPressed without a session does nothing`() {
        val sessionId = null
        val session: Session = mock()
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)
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
    fun `keep readableColor if toolbarColor is provided`() {
        val sessionManager: SessionManager = mock()
        val toolbar = BrowserToolbar(RuntimeEnvironment.application)
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
}
