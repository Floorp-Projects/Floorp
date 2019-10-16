/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.display

import android.graphics.Color
import android.view.View
import androidx.core.content.ContextCompat
import androidx.core.view.isGone
import androidx.core.view.isVisible
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.R
import mozilla.components.concept.toolbar.Toolbar.SiteSecurity
import mozilla.components.concept.toolbar.Toolbar.SiteTrackingProtection
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.processor.CollectionProcessor
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class DisplayToolbarTest {
    private fun createDisplayToolbar(): Pair<BrowserToolbar, DisplayToolbar> {
        val toolbar: BrowserToolbar = mock()
        val displayToolbar = DisplayToolbar(
            testContext, toolbar,
            View.inflate(testContext, R.layout.mozac_browser_toolbar_displaytoolbar, null)
        )
        return Pair(toolbar, displayToolbar)
    }

    @Test
    fun `clicking on the URL switches the toolbar to editing mode`() {
        val (toolbar, displayToolbar) = createDisplayToolbar()

        val urlView = displayToolbar.views.origin.urlView
        assertTrue(urlView.performClick())

        verify(toolbar).editMode()
    }

    @Test
    fun `progress is forwarded to progress bar`() {
        val (_, displayToolbar) = createDisplayToolbar()

        val progressView = displayToolbar.views.progress

        displayToolbar.updateProgress(10)
        assertEquals(10, progressView.progress)

        displayToolbar.updateProgress(50)
        assertEquals(50, progressView.progress)

        displayToolbar.updateProgress(75)
        assertEquals(75, progressView.progress)

        displayToolbar.updateProgress(100)
        assertEquals(100, progressView.progress)
    }

    @Test
    fun `trackingProtectionViewColor will change the color of the trackingProtectionIconView`() {
        val (_, displayToolbar) = createDisplayToolbar()

        assertNull(displayToolbar.views.trackingProtectionIndicator.colorFilter)

        displayToolbar.colors = displayToolbar.colors.copy(
            trackingProtection = Color.BLUE
        )

        assertNotNull(displayToolbar.views.trackingProtectionIndicator.colorFilter)
    }

    @Test
    fun `tracking protection and separator views become visible when states ON OR ACTIVE are set to siteTrackingProtection`() {
        val (_, displayToolbar) = createDisplayToolbar()

        val trackingView = displayToolbar.views.trackingProtectionIndicator
        val separatorView = displayToolbar.views.separator

        assertTrue(trackingView.visibility == View.GONE)
        assertTrue(separatorView.visibility == View.GONE)

        displayToolbar.indicators = listOf(
            DisplayToolbar.Indicators.SECURITY,
            DisplayToolbar.Indicators.TRACKING_PROTECTION
        )
        displayToolbar.url = "https://www.mozilla.org"
        displayToolbar.displayIndicatorSeparator = true
        displayToolbar.setTrackingProtectionState(SiteTrackingProtection.ON_NO_TRACKERS_BLOCKED)

        assertTrue(trackingView.isVisible)
        assertTrue(separatorView.isVisible)

        displayToolbar.setTrackingProtectionState(SiteTrackingProtection.OFF_GLOBALLY)

        assertTrue(trackingView.visibility == View.GONE)
        assertTrue(separatorView.visibility == View.GONE)

        displayToolbar.setTrackingProtectionState(SiteTrackingProtection.ON_TRACKERS_BLOCKED)

        assertTrue(trackingView.isVisible)
        assertTrue(separatorView.isVisible)
    }

    @Test
    fun `setTrackingProtectionIcons will forward to TrackingProtectionIconView`() {
        val (_, displayToolbar) = createDisplayToolbar()

        val oldTrackingProtectionIcon = displayToolbar.views.trackingProtectionIndicator.drawable
        assertNotNull(oldTrackingProtectionIcon)

        val drawable1 =
            testContext.getDrawable(TrackingProtectionIconView.DEFAULT_ICON_ON_NO_TRACKERS_BLOCKED)!!
        val drawable2 =
            testContext.getDrawable(TrackingProtectionIconView.DEFAULT_ICON_ON_TRACKERS_BLOCKED)!!
        val drawable3 =
            testContext.getDrawable(TrackingProtectionIconView.DEFAULT_ICON_OFF_FOR_A_SITE)!!

        displayToolbar.indicators = listOf(DisplayToolbar.Indicators.TRACKING_PROTECTION)
        displayToolbar.icons = displayToolbar.icons.copy(
            trackingProtectionTrackersBlocked = drawable1,
            trackingProtectionNothingBlocked = drawable2,
            trackingProtectionException = drawable3
        )

        val newTrackingProtectionIcon = displayToolbar.views.trackingProtectionIndicator.drawable
        assertNotNull(newTrackingProtectionIcon)

        assertNotEquals(
            oldTrackingProtectionIcon,
            newTrackingProtectionIcon
        )
    }

    @Test
    fun `menu view is gone by default`() {
        val (_, displayToolbar) = createDisplayToolbar()

        val menuView = displayToolbar.views.menu

        assertNotNull(menuView)
        assertTrue(menuView.visibility == View.GONE)
    }

    @Test
    fun `menu view becomes visible once a menu builder is set`() {
        val (_, displayToolbar) = createDisplayToolbar()

        val menuView = displayToolbar.views.menu

        assertNotNull(menuView)

        assertTrue(menuView.visibility == View.GONE)

        displayToolbar.menuBuilder = BrowserMenuBuilder(emptyList())

        assertTrue(menuView.visibility == View.VISIBLE)

        displayToolbar.menuBuilder = null

        assertTrue(menuView.visibility == View.GONE)
    }

    @Test
    fun `no menu builder is set by default`() {
        val (_, displayToolbar) = createDisplayToolbar()

        assertNull(displayToolbar.menuBuilder)
    }

    @Test
    fun `menu builder will be used to create and show menu when button is clicked`() {
        val (_, displayToolbar) = createDisplayToolbar()
        val menuView = displayToolbar.views.menu

        val menuBuilder = mock(BrowserMenuBuilder::class.java)
        val menu = mock(BrowserMenu::class.java)
        doReturn(menu).`when`(menuBuilder).build(testContext)

        displayToolbar.menuBuilder = menuBuilder

        verify(menuBuilder, never()).build(testContext)
        verify(menu, never()).show(menuView)

        menuView.performClick()

        verify(menuBuilder).build(testContext)
        verify(menu).show(eq(menuView), any(), anyBoolean(), any())
        verify(menu, never()).invalidate()

        displayToolbar.invalidateActions()

        verify(menu).invalidate()
    }

    @Test
    fun `browser action gets added as view to toolbar`() {
        val contentDescription = "Mozilla"

        val (_, displayToolbar) = createDisplayToolbar()

        assertEquals(0, displayToolbar.views.browserActions.childCount)

        val action = BrowserToolbar.Button(mock(), contentDescription) {}
        displayToolbar.addBrowserAction(action)

        assertEquals(1, displayToolbar.views.browserActions.childCount)

        val view = displayToolbar.views.browserActions.getChildAt(0)
        assertEquals(contentDescription, view.contentDescription)
    }

    @Test
    fun `clicking browser action view triggers listener of action`() {
        var callbackExecuted = false

        val action = BrowserToolbar.Button(mock(), "Button") {
            callbackExecuted = true
        }

        val (_, displayToolbar) = createDisplayToolbar()
        displayToolbar.addBrowserAction(action)

        assertEquals(1, displayToolbar.views.browserActions.childCount)
        val view = displayToolbar.views.browserActions.getChildAt(0)

        assertNotNull(view)

        assertFalse(callbackExecuted)

        view?.performClick()

        assertTrue(callbackExecuted)
    }

    @Test
    fun `page actions will be added as view to the toolbar`() {
        val (_, displayToolbar) = createDisplayToolbar()

        assertEquals(0, displayToolbar.views.pageActions.childCount)

        val action = BrowserToolbar.Button(mock(), "Reader Mode") {}
        displayToolbar.addPageAction(action)

        assertEquals(1, displayToolbar.views.pageActions.childCount)
        val view = displayToolbar.views.pageActions.getChildAt(0)
        assertEquals("Reader Mode", view.contentDescription)
    }

    @Test
    fun `clicking a page action view will execute the listener of the action`() {
        var listenerExecuted = false

        val action = BrowserToolbar.Button(mock(), "Reload") {
            listenerExecuted = true
        }

        val (_, displayToolbar) = createDisplayToolbar()
        displayToolbar.addPageAction(action)

        assertFalse(listenerExecuted)

        assertEquals(1, displayToolbar.views.pageActions.childCount)
        val view = displayToolbar.views.pageActions.getChildAt(0)

        assertNotNull(view)
        view!!.performClick()

        assertTrue(listenerExecuted)
    }

    @Test
    fun `navigation actions will be added as view to the toolbar`() {
        val (_, displayToolbar) = createDisplayToolbar()

        assertEquals(0, displayToolbar.views.navigationActions.childCount)

        displayToolbar.addNavigationAction(BrowserToolbar.Button(mock(), "Back") {})
        displayToolbar.addNavigationAction(BrowserToolbar.Button(mock(), "Forward") {})

        assertEquals(2, displayToolbar.views.navigationActions.childCount)
    }

    @Test
    fun `clicking on navigation action will execute listener of the action`() {
        val (_, displayToolbar) = createDisplayToolbar()

        var listenerExecuted = false
        val action = BrowserToolbar.Button(mock(), "Back") {
            listenerExecuted = true
        }

        displayToolbar.addNavigationAction(action)

        assertFalse(listenerExecuted)

        assertEquals(1, displayToolbar.views.navigationActions.childCount)
        val view = displayToolbar.views.navigationActions.getChildAt(0)
        view.performClick()

        assertTrue(listenerExecuted)
    }

    @Test
    fun `view of not visible navigation action gets removed after invalidating`() {
        val (_, displayToolbar) = createDisplayToolbar()

        var shouldActionBeDisplayed = true

        val action = BrowserToolbar.Button(
            mock(),
            "Back",
            visible = { shouldActionBeDisplayed }
        ) { /* Do nothing */ }

        displayToolbar.addNavigationAction(action)

        assertEquals(1, displayToolbar.views.navigationActions.childCount)

        shouldActionBeDisplayed = false
        displayToolbar.invalidateActions()

        assertEquals(0, displayToolbar.views.navigationActions.childCount)

        shouldActionBeDisplayed = true
        displayToolbar.invalidateActions()

        assertEquals(1, displayToolbar.views.navigationActions.childCount)
    }

    @Test
    fun `toolbar should call bind with view argument on action after invalidating`() {
        val (_, displayToolbar) = createDisplayToolbar()

        val action = spy(BrowserToolbar.Button(mock(), "Reload") {})

        displayToolbar.addPageAction(action)

        assertEquals(1, displayToolbar.views.pageActions.childCount)
        val view = displayToolbar.views.pageActions.getChildAt(0)

        verify(action, never()).bind(view!!)

        displayToolbar.invalidateActions()

        verify(action).bind(view)
    }

    @Test
    fun `page action will not be added if visible lambda of action returns false`() {
        val (_, displayToolbar) = createDisplayToolbar()

        val visibleAction = BrowserToolbar.Button(mock(), "Reload") {}
        val invisibleAction = BrowserToolbar.Button(
            mock(),
            "Reader Mode",
            visible = { false }) {}

        displayToolbar.addPageAction(visibleAction)
        displayToolbar.addPageAction(invisibleAction)

        assertEquals(1, displayToolbar.views.pageActions.childCount)

        val view = displayToolbar.views.pageActions.getChildAt(0)
        assertEquals("Reload", view.contentDescription)
    }

    @Test
    fun `browser action will not be added if visible lambda of action returns false`() {
        val (_, displayToolbar) = createDisplayToolbar()

        val visibleAction = BrowserToolbar.Button(mock(), "Tabs") {}
        val invisibleAction = BrowserToolbar.Button(
            mock(),
            "Settings",
            visible = { false }) {}

        displayToolbar.addBrowserAction(visibleAction)
        displayToolbar.addBrowserAction(invisibleAction)

        assertEquals(1, displayToolbar.views.browserActions.childCount)

        val view = displayToolbar.views.browserActions.getChildAt(0)
        assertEquals("Tabs", view.contentDescription)
    }

    @Test
    fun `navigation action will not be added if visible lambda of action returns false`() {
        val (_, displayToolbar) = createDisplayToolbar()

        val visibleAction = BrowserToolbar.Button(mock(), "Forward") {}
        val invisibleAction = BrowserToolbar.Button(
            mock(),
            "Back",
            visible = { false }) {}

        displayToolbar.addNavigationAction(visibleAction)
        displayToolbar.addNavigationAction(invisibleAction)

        assertEquals(1, displayToolbar.views.navigationActions.childCount)

        val view = displayToolbar.views.navigationActions.getChildAt(0)
        assertEquals("Forward", view.contentDescription)
    }

    @Test
    fun `url background will be added and removed from display layout`() {
        val (_, displayToolbar) = createDisplayToolbar()

        assertNull(displayToolbar.views.background.drawable)

        displayToolbar.setUrlBackground(
            ContextCompat.getDrawable(testContext, R.drawable.mozac_ic_broken_lock)
        )

        assertNotNull(displayToolbar.views.background.drawable)

        displayToolbar.setUrlBackground(null)

        assertNull(displayToolbar.views.background.drawable)
    }

    @Test
    fun `titleView does not display when there is no title text`() {
        val (_, displayToolbar) = createDisplayToolbar()

        assertTrue(displayToolbar.views.origin.titleView.isGone)

        displayToolbar.title = "Hello World"

        assertTrue(displayToolbar.views.origin.titleView.isVisible)
    }

    @Test
    fun `toolbar only switches to editing mode if onUrlClicked returns true`() {
        val (toolbar, displayToolbar) = createDisplayToolbar()

        displayToolbar.views.origin.urlView.performClick()

        verify(toolbar).editMode()

        reset(toolbar)
        displayToolbar.onUrlClicked = { false }
        displayToolbar.views.origin.urlView.performClick()

        verify(toolbar, never()).editMode()

        reset(toolbar)
        displayToolbar.onUrlClicked = { true }
        displayToolbar.views.origin.urlView.performClick()

        verify(toolbar).editMode()
    }

    @Test
    fun `urlView delegates long click when set`() {
        val (_, displayToolbar) = createDisplayToolbar()

        var longUrlClicked = false

        displayToolbar.setOnUrlLongClickListener {
            longUrlClicked = true
            false
        }

        assertFalse(longUrlClicked)
        displayToolbar.views.origin.urlView.performLongClick()
        assertTrue(longUrlClicked)
    }

    @Test
    fun `urlView longClickListener can be unset`() {
        val (_, displayToolbar) = createDisplayToolbar()

        var longClicked = false
        displayToolbar.setOnUrlLongClickListener {
            longClicked = true
            true
        }

        displayToolbar.views.origin.urlView.performLongClick()
        assertTrue(longClicked)
        longClicked = false

        displayToolbar.setOnUrlLongClickListener(null)
        displayToolbar.views.origin.urlView.performLongClick()

        assertFalse(longClicked)
    }

    @Test
    fun `iconView changes site secure state when site security changes`() {
        val (_, displayToolbar) = createDisplayToolbar()
        assertEquals(SiteSecurity.INSECURE, displayToolbar.views.securityIndicator.siteSecurity)

        displayToolbar.siteSecurity = SiteSecurity.SECURE

        assertEquals(SiteSecurity.SECURE, displayToolbar.views.securityIndicator.siteSecurity)

        displayToolbar.siteSecurity = SiteSecurity.INSECURE

        assertEquals(SiteSecurity.INSECURE, displayToolbar.views.securityIndicator.siteSecurity)
    }

    @Test
    fun `securityIconColor is set when securityIconColor changes`() {
        val (_, displayToolbar) = createDisplayToolbar()

        assertNull(displayToolbar.views.securityIndicator.colorFilter)

        displayToolbar.colors = displayToolbar.colors.copy(
            securityIconSecure = Color.TRANSPARENT,
            securityIconInsecure = Color.TRANSPARENT
        )

        assertNull(displayToolbar.views.securityIndicator.colorFilter)

        displayToolbar.colors = displayToolbar.colors.copy(
            securityIconSecure = Color.BLUE,
            securityIconInsecure = Color.BLUE
        )

        assertNotNull(displayToolbar.views.securityIndicator.colorFilter)
    }

    @Test
    fun `clicking menu button emits facts with additional extras from builder set`() {
        CollectionProcessor.withFactCollection { facts ->
            val (_, displayToolbar) = createDisplayToolbar()
            val menuView = displayToolbar.views.menu

            val menuBuilder = BrowserMenuBuilder(
                listOf(SimpleBrowserMenuItem("Mozilla")), mapOf(
                    "customTab" to true,
                    "test" to "23"
                )
            )
            displayToolbar.menuBuilder = menuBuilder

            assertEquals(0, facts.size)

            menuView.performClick()

            assertEquals(1, facts.size)

            val fact = facts[0]

            assertEquals(Component.BROWSER_TOOLBAR, fact.component)
            assertEquals(Action.CLICK, fact.action)
            assertEquals("menu", fact.item)
            assertNull(fact.value)

            assertNotNull(fact.metadata)

            val metadata = fact.metadata!!
            assertEquals(2, metadata.size)
            assertTrue(metadata.containsKey("customTab"))
            assertTrue(metadata.containsKey("test"))
            assertEquals(true, metadata["customTab"])
            assertEquals("23", metadata["test"])
        }
    }

    @Test
    fun `clicking on site security indicator invokes listener`() {
        var listenerInvoked = false

        val (_, displayToolbar) = createDisplayToolbar()

        assertNull(displayToolbar.views.securityIndicator.background)

        displayToolbar.setOnSiteSecurityClickedListener {
            listenerInvoked = true
        }

        assertNotNull(displayToolbar.views.securityIndicator.background)

        displayToolbar.views.securityIndicator.performClick()

        assertTrue(listenerInvoked)

        listenerInvoked = false

        displayToolbar.setOnSiteSecurityClickedListener { }

        assertNotNull(displayToolbar.views.securityIndicator.background)

        displayToolbar.views.securityIndicator.performClick()

        assertFalse(listenerInvoked)

        displayToolbar.setOnSiteSecurityClickedListener(null)

        assertNull(displayToolbar.views.securityIndicator.background)
    }

    @Test
    fun `Backgrounding the app dismisses menu if already open`() {
        val (_, displayToolbar) = createDisplayToolbar()
        val menuView = displayToolbar.views.menu

        menuView.menu = mock()
        displayToolbar.onStop()

        verify(menuView.menu)?.dismiss()
    }
}
