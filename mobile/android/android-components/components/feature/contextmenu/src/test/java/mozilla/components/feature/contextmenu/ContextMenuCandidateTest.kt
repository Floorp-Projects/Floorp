/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.contextmenu

import android.content.ClipboardManager
import android.content.Context
import android.view.View
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.action.SystemAction
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.HitResult
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.any
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class ContextMenuCandidateTest {

    private lateinit var snackbarDelegate: TestSnackbarDelegate

    @Before
    fun setUp() {
        snackbarDelegate = TestSnackbarDelegate()
    }

    @Test
    fun `Default candidates sanity check`() {
        val candidates = ContextMenuCandidate.defaultCandidates(testContext, mock(), mock(), mock())
        // Just a sanity check: When changing the list of default candidates be aware that this will affect all
        // consumers of this component using the default list.
        assertEquals(7, candidates.size)
    }

    @Test
    fun `Candidate "Open Link in New Tab" showFor displayed in correct cases`() {
        val sessionManager = spy(SessionManager(mock()))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any())
        val tabsUseCases = TabsUseCases(sessionManager)
        val parentView = CoordinatorLayout(testContext)
        val openInNewTab = ContextMenuCandidate.createOpenInNewTabCandidate(
                testContext, tabsUseCases, parentView, snackbarDelegate)

        assertTrue(openInNewTab.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertFalse(openInNewTab.showFor(
            createTab("https://www.mozilla.org", private = true),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertTrue(openInNewTab.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.IMAGE_SRC("https://www.mozilla.org", "https://www.mozilla.org")))

        assertFalse(openInNewTab.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.IMAGE("https://www.mozilla.org")))

        assertFalse(openInNewTab.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.VIDEO("https://www.mozilla.org")))
    }

    @Test
    fun `Candidate "Open Link in New Tab" action properly executes and shows snackbar`() {
        val store = BrowserStore()
        val sessionManager = spy(SessionManager(mock(), store))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any())
        sessionManager.add(Session("https://www.mozilla.org"))

        val tabsUseCases = TabsUseCases(sessionManager)
        val parentView = CoordinatorLayout(testContext)
        val openInNewTab = ContextMenuCandidate.createOpenInNewTabCandidate(
            testContext, tabsUseCases, parentView, snackbarDelegate)

        assertEquals(1, store.state.tabs.size)
        assertFalse(snackbarDelegate.hasShownSnackbar)

        openInNewTab.action.invoke(store.state.tabs.first(), HitResult.UNKNOWN("https://firefox.com"))

        assertEquals(2, store.state.tabs.size)
        assertTrue(snackbarDelegate.hasShownSnackbar)
        assertNotNull(snackbarDelegate.lastActionListener)
        assertEquals("https://firefox.com", store.state.tabs.last().content.url)
    }

    @Test
    fun `Candidate "Open Link in New Tab" snackbar action works`() {
        val store = BrowserStore()
        val sessionManager = spy(SessionManager(mock(), store))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any())
        sessionManager.add(Session("https://www.mozilla.org"))

        val tabsUseCases = TabsUseCases(sessionManager)
        val parentView = CoordinatorLayout(testContext)

        val openInNewTab = ContextMenuCandidate.createOpenInNewTabCandidate(
                testContext, tabsUseCases, parentView, snackbarDelegate)

        assertEquals(1, store.state.tabs.size)
        assertFalse(snackbarDelegate.hasShownSnackbar)

        openInNewTab.action.invoke(store.state.tabs.first(), HitResult.UNKNOWN("https://firefox.com"))
        assertEquals("https://www.mozilla.org", store.state.selectedTab!!.content.url)

        snackbarDelegate.lastActionListener!!.invoke(mock())

        assertEquals("https://firefox.com", store.state.selectedTab!!.content.url)
    }

    @Test
    fun `Candidate "Open Link in New Tab" action properly handles link with an image`() {
        val store = BrowserStore()
        val sessionManager = spy(SessionManager(mock(), store))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any())
        sessionManager.add(Session("https://www.mozilla.org"))

        val tabsUseCases = TabsUseCases(sessionManager)
        val parentView = CoordinatorLayout(testContext)

        val openInNewTab = ContextMenuCandidate.createOpenInNewTabCandidate(
            testContext, tabsUseCases, parentView, snackbarDelegate)

        assertEquals(1, store.state.tabs.size)
        openInNewTab.action.invoke(
            store.state.tabs.first(),
            HitResult.IMAGE_SRC("https://www.mozilla_src.org", "https://www.mozilla_uri.org"))

        assertEquals("https://www.mozilla_uri.org", store.state.tabs.last().content.url)
    }

    /* Private */
    @Test
    fun `Candidate "Open Link in Private Tab" showFor displayed in correct cases`() {
        val store = BrowserStore()
        val sessionManager = spy(SessionManager(mock(), store))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any())
        sessionManager.add(Session("https://www.mozilla.org"))

        val tabsUseCases = TabsUseCases(sessionManager)
        val parentView = CoordinatorLayout(testContext)
        val openInPrivateTab = ContextMenuCandidate.createOpenInPrivateTabCandidate(
                testContext, tabsUseCases, parentView, snackbarDelegate)

        assertTrue(openInPrivateTab.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertTrue(openInPrivateTab.showFor(
            createTab("https://www.mozilla.org", private = true),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertTrue(openInPrivateTab.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.IMAGE_SRC("https://www.mozilla.org", "https://www.mozilla.org")))

        assertFalse(openInPrivateTab.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.IMAGE("https://www.mozilla.org")))

        assertFalse(openInPrivateTab.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.VIDEO("https://www.mozilla.org")))
    }

    @Test
    fun `Candidate "Open Link in Private Tab" action properly executes and shows snackbar`() {
        val store = BrowserStore()
        val sessionManager = spy(SessionManager(mock(), store))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any())
        sessionManager.add(Session("https://www.mozilla.org"))

        val tabsUseCases = TabsUseCases(sessionManager)
        val parentView = CoordinatorLayout(testContext)
        val openInPrivateTab = ContextMenuCandidate.createOpenInPrivateTabCandidate(
                testContext, tabsUseCases, parentView, snackbarDelegate)

        assertEquals(1, store.state.tabs.size)
        assertFalse(snackbarDelegate.hasShownSnackbar)

        openInPrivateTab.action.invoke(store.state.tabs.first(), HitResult.UNKNOWN("https://firefox.com"))

        assertEquals(2, store.state.tabs.size)
        assertTrue(snackbarDelegate.hasShownSnackbar)
        assertNotNull(snackbarDelegate.lastActionListener)
        assertEquals("https://firefox.com", store.state.tabs.last().content.url)
    }

    @Test
    fun `Candidate "Open Link in Private Tab" snackbar action works`() {
        val store = BrowserStore()
        val sessionManager = spy(SessionManager(mock(), store))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any())
        sessionManager.add(Session("https://www.mozilla.org"))

        val tabsUseCases = TabsUseCases(sessionManager)
        val parentView = CoordinatorLayout(testContext)
        val openInPrivateTab = ContextMenuCandidate.createOpenInPrivateTabCandidate(
                testContext, tabsUseCases, parentView, snackbarDelegate)

        assertEquals(1, store.state.tabs.size)
        assertFalse(snackbarDelegate.hasShownSnackbar)

        openInPrivateTab.action.invoke(store.state.tabs.first(), HitResult.UNKNOWN("https://firefox.com"))
        assertEquals("https://www.mozilla.org", store.state.selectedTab!!.content.url)

        snackbarDelegate.lastActionListener!!.invoke(mock())

        assertEquals("https://firefox.com", store.state.selectedTab!!.content.url)
    }

    @Test
    fun `Candidate "Open Link in Private Tab" action properly handles link with an image`() {
        val store = BrowserStore()
        val sessionManager = spy(SessionManager(mock(), store))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any())
        sessionManager.add(Session("https://www.mozilla.org"))

        val tabsUseCases = TabsUseCases(sessionManager)
        val parentView = CoordinatorLayout(testContext)
        val openInPrivateTab = ContextMenuCandidate.createOpenInPrivateTabCandidate(
                testContext, tabsUseCases, parentView, snackbarDelegate)

        assertEquals(1, store.state.tabs.size)
        openInPrivateTab.action.invoke(
            store.state.tabs.first(),
            HitResult.IMAGE_SRC("https://www.mozilla_src.org", "https://www.mozilla_uri.org"))
        assertEquals("https://www.mozilla_uri.org", store.state.tabs.last().content.url)
    }

    @Test
    fun `Candidate "Open Image in New Tab"`() {
        val store = BrowserStore()
        val sessionManager = spy(SessionManager(mock(), store))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any())
        sessionManager.add(Session("https://www.mozilla.org"))

        val tabsUseCases = TabsUseCases(sessionManager)
        val parentView = CoordinatorLayout(testContext)

        val openImageInTab = ContextMenuCandidate.createOpenImageInNewTabCandidate(
            testContext, tabsUseCases, parentView, snackbarDelegate)

        // showFor

        assertFalse(openImageInTab.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertFalse(openImageInTab.showFor(
            createTab("https://www.mozilla.org", private = true),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertTrue(openImageInTab.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.IMAGE_SRC("https://www.mozilla.org", "https://www.mozilla.org")))

        assertTrue(openImageInTab.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.IMAGE("https://www.mozilla.org")))

        assertFalse(openImageInTab.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.VIDEO("https://www.mozilla.org")))

        // action

        assertEquals(1, store.state.tabs.size)
        assertFalse(snackbarDelegate.hasShownSnackbar)

        openImageInTab.action.invoke(
            store.state.tabs.first(),
            HitResult.IMAGE_SRC("https://firefox.com", "https://getpocket.com"))

        assertEquals(2, store.state.tabs.size)
        assertFalse(store.state.tabs.last().content.private)
        assertEquals("https://firefox.com", store.state.tabs.last().content.url)
        assertTrue(snackbarDelegate.hasShownSnackbar)
        assertNotNull(snackbarDelegate.lastActionListener)

        // Snackbar action

        assertEquals("https://www.mozilla.org", store.state.selectedTab!!.content.url)

        snackbarDelegate.lastActionListener!!.invoke(mock())

        assertEquals("https://firefox.com", store.state.selectedTab!!.content.url)
    }

    @Test
    fun `Candidate "Open Image in New Tab" opens in private tab if session is private`() {
        val store = BrowserStore()
        val sessionManager = spy(SessionManager(mock(), store))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any())
        sessionManager.add(Session("https://www.mozilla.org", private = true))

        val tabsUseCases = TabsUseCases(sessionManager)
        val parentView = CoordinatorLayout(testContext)

        val openImageInTab = ContextMenuCandidate.createOpenImageInNewTabCandidate(
            testContext, tabsUseCases, parentView, snackbarDelegate)

        assertEquals(1, store.state.tabs.size)

        openImageInTab.action.invoke(
            store.state.tabs.first(),
            HitResult.IMAGE_SRC("https://firefox.com", "https://getpocket.com"))

        assertEquals(2, store.state.tabs.size)
        assertTrue(store.state.tabs.last().content.private)
        assertEquals("https://firefox.com", store.state.tabs.last().content.url)
    }

    @Test
    fun `Candidate "Save image"`() {
        val store = BrowserStore()
        val sessionManager = spy(SessionManager(mock(), store))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any())
        sessionManager.add(Session("https://www.mozilla.org", private = true))

        val saveImage = ContextMenuCandidate.createSaveImageCandidate(
            testContext,
            ContextMenuUseCases(sessionManager, store))

        // showFor

        assertFalse(saveImage.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertFalse(saveImage.showFor(
            createTab("https://www.mozilla.org", private = true),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertTrue(saveImage.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.IMAGE_SRC("https://www.mozilla.org", "https://www.mozilla.org")))

        assertTrue(saveImage.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.IMAGE("https://www.mozilla.org")))

        assertFalse(saveImage.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.VIDEO("https://www.mozilla.org")))

        // action

        assertNull(store.state.tabs.first().content.download)

        saveImage.action.invoke(
            store.state.tabs.first(),
            HitResult.IMAGE_SRC("https://www.mozilla.org/media/img/logos/firefox/logo-quantum.9c5e96634f92.png",
                "https://firefox.com"))

        // Dispatching a random unrelated action to block on it in order to wait for all other
        // dispatched actions to have completed.
        store.dispatch(SystemAction.LowMemoryAction).joinBlocking()

        assertNotNull(store.state.tabs.first().content.download)
        assertEquals(
            "https://www.mozilla.org/media/img/logos/firefox/logo-quantum.9c5e96634f92.png",
            store.state.tabs.first().content.download!!.url)
    }

    @Test
    fun `Candidate "Share Link"`() {
        val context = spy(testContext)

        val shareLink = ContextMenuCandidate.createShareLinkCandidate(context)

        // showFor

        assertTrue(shareLink.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertTrue(shareLink.showFor(
            createTab("https://www.mozilla.org", private = true),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertTrue(shareLink.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.IMAGE_SRC("https://www.mozilla.org", "https://www.mozilla.org")))

        assertFalse(shareLink.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.IMAGE("https://www.mozilla.org")))

        assertFalse(shareLink.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.VIDEO("https://www.mozilla.org")))

        // action

        val store = BrowserStore()
        val sessionManager = spy(SessionManager(mock(), store))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any())
        sessionManager.add(Session("https://www.mozilla.org", private = true))

        shareLink.action.invoke(
            store.state.tabs.first(),
            HitResult.IMAGE_SRC("https://firefox.com", "https://getpocket.com"))

        verify(context).startActivity(any())
    }

    @Test
    fun `Candidate "Copy Link"`() {
        val parentView = CoordinatorLayout(testContext)

        val copyLink = ContextMenuCandidate.createCopyLinkCandidate(
            testContext, parentView, snackbarDelegate)

        // showFor

        assertTrue(copyLink.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertTrue(copyLink.showFor(
            createTab("https://www.mozilla.org", private = true),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertTrue(copyLink.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.IMAGE_SRC("https://www.mozilla.org", "https://www.mozilla.org")))

        assertFalse(copyLink.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.IMAGE("https://www.mozilla.org")))

        assertFalse(copyLink.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.VIDEO("https://www.mozilla.org")))

        // action

        val store = BrowserStore()
        val sessionManager = spy(SessionManager(mock(), store))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any())
        sessionManager.add(Session("https://www.mozilla.org", private = true))

        copyLink.action.invoke(
            store.state.tabs.first(),
            HitResult.IMAGE_SRC("https://firefox.com", "https://getpocket.com"))

        assertTrue(snackbarDelegate.hasShownSnackbar)

        val clipboardManager = testContext.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        assertEquals(
            "https://getpocket.com",
            clipboardManager.primaryClip!!.getItemAt(0).text
        )
    }

    @Test
    fun `Candidate "Copy Image Location"`() {
        val parentView = CoordinatorLayout(testContext)

        val copyImageLocation = ContextMenuCandidate.createCopyImageLocationCandidate(
            testContext, parentView, snackbarDelegate)

        // showFor

        assertFalse(copyImageLocation.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertFalse(copyImageLocation.showFor(
            createTab("https://www.mozilla.org", private = true),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertTrue(copyImageLocation.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.IMAGE_SRC("https://www.mozilla.org", "https://www.mozilla.org")))

        assertTrue(copyImageLocation.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.IMAGE("https://www.mozilla.org")))

        assertFalse(copyImageLocation.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.VIDEO("https://www.mozilla.org")))

        // action

        val store = BrowserStore()
        val sessionManager = spy(SessionManager(mock(), store))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any())
        sessionManager.add(Session("https://www.mozilla.org", private = true))

        copyImageLocation.action.invoke(
            store.state.tabs.first(),
            HitResult.IMAGE_SRC("https://firefox.com", "https://getpocket.com"))

        assertTrue(snackbarDelegate.hasShownSnackbar)

        val clipboardManager = testContext.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        assertEquals(
            "https://firefox.com",
            clipboardManager.primaryClip!!.getItemAt(0).text
        )
    }
}

private class TestSnackbarDelegate : ContextMenuCandidate.SnackbarDelegate {
    var hasShownSnackbar = false
    var lastActionListener: ((v: View) -> Unit)? = null

    override fun show(snackBarParentView: View, text: Int, duration: Int, action: Int, listener: ((v: View) -> Unit)?) {
        hasShownSnackbar = true
        lastActionListener = listener
    }
}