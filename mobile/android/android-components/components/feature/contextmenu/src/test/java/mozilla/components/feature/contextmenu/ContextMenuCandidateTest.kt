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
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.HitResult
import mozilla.components.feature.app.links.AppLinkRedirect
import mozilla.components.feature.app.links.AppLinksUseCases
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.libstate.ext.waitUntilIdle
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
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
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
        assertEquals(13, candidates.size)
    }

    @Test
    fun `Candidate "Open Link in New Tab" showFor displayed in correct cases`() {
        val store = BrowserStore()
        val sessionManager = spy(SessionManager(mock(), store))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any(), anyBoolean())
        val tabsUseCases = TabsUseCases(store, sessionManager)
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
    fun `Candidate "Open Link in New Tab" action properly executes for session with a contextId`() {
        val store = BrowserStore()
        val sessionManager = spy(SessionManager(mock(), store))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any(), anyBoolean())
        sessionManager.add(Session("https://www.mozilla.org", contextId = "1"))

        val tabsUseCases = TabsUseCases(store, sessionManager)
        val parentView = CoordinatorLayout(testContext)
        val openInNewTab = ContextMenuCandidate.createOpenInNewTabCandidate(
            testContext, tabsUseCases, parentView, snackbarDelegate)

        assertEquals(1, store.state.tabs.size)
        assertEquals("1", store.state.tabs.first().contextId)

        openInNewTab.action.invoke(store.state.tabs.first(), HitResult.UNKNOWN("https://firefox.com"))

        assertEquals(2, store.state.tabs.size)
        assertEquals("https://firefox.com", store.state.tabs.last().content.url)
        assertEquals("1", store.state.tabs.last().contextId)
    }

    @Test
    fun `Candidate "Open Link in New Tab" action properly executes and shows snackbar`() {
        val store = BrowserStore()
        val sessionManager = spy(SessionManager(mock(), store))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any(), anyBoolean())
        sessionManager.add(Session("https://www.mozilla.org"))

        val tabsUseCases = TabsUseCases(store, sessionManager)
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
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any(), anyBoolean())
        sessionManager.add(Session("https://www.mozilla.org"))

        val tabsUseCases = TabsUseCases(store, sessionManager)
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
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any(), anyBoolean())
        sessionManager.add(Session("https://www.mozilla.org"))

        val tabsUseCases = TabsUseCases(store, sessionManager)
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
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any(), anyBoolean())
        sessionManager.add(Session("https://www.mozilla.org"))

        val tabsUseCases = TabsUseCases(store, sessionManager)
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
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any(), anyBoolean())
        sessionManager.add(Session("https://www.mozilla.org"))

        val tabsUseCases = TabsUseCases(store, sessionManager)
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
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any(), anyBoolean())
        sessionManager.add(Session("https://www.mozilla.org"))

        val tabsUseCases = TabsUseCases(store, sessionManager)
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
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any(), anyBoolean())
        sessionManager.add(Session("https://www.mozilla.org"))

        val tabsUseCases = TabsUseCases(store, sessionManager)
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
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any(), anyBoolean())
        sessionManager.add(Session("https://www.mozilla.org"))

        val tabsUseCases = TabsUseCases(store, sessionManager)
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
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any(), anyBoolean())
        sessionManager.add(Session("https://www.mozilla.org", private = true))

        val tabsUseCases = TabsUseCases(store, sessionManager)
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
    fun `Candidate "Open Image in New Tab" opens with the session's contextId`() {
        val store = BrowserStore()
        val sessionManager = spy(SessionManager(mock(), store))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any(), anyBoolean())
        sessionManager.add(Session("https://www.mozilla.org", contextId = "1"))

        val tabsUseCases = TabsUseCases(store, sessionManager)
        val parentView = CoordinatorLayout(testContext)

        val openImageInTab = ContextMenuCandidate.createOpenImageInNewTabCandidate(
            testContext, tabsUseCases, parentView, snackbarDelegate)

        assertEquals(1, store.state.tabs.size)
        assertEquals("1", store.state.tabs.first().contextId)

        openImageInTab.action.invoke(
            store.state.tabs.first(),
            HitResult.IMAGE_SRC("https://firefox.com", "https://getpocket.com"))

        assertEquals(2, store.state.tabs.size)
        assertEquals("https://firefox.com", store.state.tabs.last().content.url)
        assertEquals("1", store.state.tabs.last().contextId)
    }

    @Test
    fun `Candidate "Save image"`() {
        val store = BrowserStore()
        val sessionManager = spy(SessionManager(mock(), store))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any(), anyBoolean())
        sessionManager.add(Session("https://www.mozilla.org", private = true))

        val saveImage = ContextMenuCandidate.createSaveImageCandidate(
            testContext,
            ContextMenuUseCases(store))

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

        store.waitUntilIdle()

        assertNotNull(store.state.tabs.first().content.download)
        assertEquals(
            "https://www.mozilla.org/media/img/logos/firefox/logo-quantum.9c5e96634f92.png",
            store.state.tabs.first().content.download!!.url)
        assertTrue(
            store.state.tabs.first().content.download!!.skipConfirmation)
    }

    @Test
    fun `Candidate "Save video and audio"`() {
        val store = BrowserStore()
        val sessionManager = spy(SessionManager(mock(), store))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any(), anyBoolean())
        sessionManager.add(Session("https://www.mozilla.org", private = true))

        val saveVideoAudio = ContextMenuCandidate.createSaveVideoAudioCandidate(
            testContext,
            ContextMenuUseCases(store))

        // showFor

        assertFalse(saveVideoAudio.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertFalse(saveVideoAudio.showFor(
            createTab("https://www.mozilla.org", private = true),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertFalse(saveVideoAudio.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.IMAGE_SRC("https://www.mozilla.org", "https://www.mozilla.org")))

        assertFalse(saveVideoAudio.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.IMAGE("https://www.mozilla.org")))

        assertTrue(saveVideoAudio.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.VIDEO("https://www.mozilla.org")))

        assertTrue(saveVideoAudio.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.AUDIO("https://www.mozilla.org")))

        // action

        assertNull(store.state.tabs.first().content.download)

        saveVideoAudio.action.invoke(
            store.state.tabs.first(),
            HitResult.AUDIO("https://developer.mozilla.org/media/examples/t-rex-roar.mp3"))

        store.waitUntilIdle()

        assertNotNull(store.state.tabs.first().content.download)
        assertEquals(
            "https://developer.mozilla.org/media/examples/t-rex-roar.mp3",
            store.state.tabs.first().content.download!!.url)
        assertTrue(
            store.state.tabs.first().content.download!!.skipConfirmation)
    }

    @Test
    fun `Candidate "download link"`() {
        val store = BrowserStore()
        val sessionManager = spy(SessionManager(mock(), store))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any(), anyBoolean())
        sessionManager.add(Session("https://www.mozilla.org", private = true))

        val downloadLink = ContextMenuCandidate.createDownloadLinkCandidate(
            testContext,
            ContextMenuUseCases(store)
        )

        // showFor

        assertTrue(downloadLink.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertTrue(downloadLink.showFor(
            createTab("https://www.mozilla.org", private = true),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertTrue(downloadLink.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.IMAGE_SRC("https://www.mozilla.org", "https://www.mozilla.org")))

        assertFalse(downloadLink.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.IMAGE("https://www.mozilla.org")))

        assertFalse(downloadLink.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.VIDEO("https://www.mozilla.org")))

        assertFalse(downloadLink.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.PHONE("https://www.mozilla.org")))

        assertFalse(downloadLink.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.EMAIL("https://www.mozilla.org")))

        assertFalse(downloadLink.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.GEO("https://www.mozilla.org")))

        assertFalse(downloadLink.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.UNKNOWN("https://www.mozilla.org/firefox/products.html")))

        assertFalse(downloadLink.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.UNKNOWN("https://www.mozilla.org/firefox/products.htm")))

        // action

        assertNull(store.state.tabs.first().content.download)

        downloadLink.action.invoke(
            store.state.tabs.first(),
            HitResult.IMAGE_SRC("https://www.mozilla.org/media/img/logos/firefox/logo-quantum.9c5e96634f92.png",
                "https://firefox.com"))

        store.waitUntilIdle()

        assertNotNull(store.state.tabs.first().content.download)
        assertEquals(
            "https://www.mozilla.org/media/img/logos/firefox/logo-quantum.9c5e96634f92.png",
            store.state.tabs.first().content.download!!.url)
        assertTrue(
            store.state.tabs.first().content.download!!.skipConfirmation)
    }

    @Test
    fun `Get link for image, video, audio gets title if title is set`() {
        val titleString = "test title"

        val hitResultImage = HitResult.IMAGE("https://www.mozilla.org", titleString)
        var title = hitResultImage.getLink()
        assertEquals(titleString, title)

        val hitResultVideo = HitResult.VIDEO("https://www.mozilla.org", titleString)
        title = hitResultVideo.getLink()
        assertEquals(titleString, title)

        val hitResultAudio = HitResult.AUDIO("https://www.mozilla.org", titleString)
        title = hitResultAudio.getLink()
        assertEquals(titleString, title)
    }

    @Test
    fun `Get link for image, video, audio gets URL if title is blank`() {
        val titleString = " "
        val url = "https://www.mozilla.org"

        val hitResultImage = HitResult.IMAGE(url, titleString)
        var title = hitResultImage.getLink()
        assertEquals(url, title)

        val hitResultVideo = HitResult.VIDEO(url, titleString)
        title = hitResultVideo.getLink()
        assertEquals(url, title)

        val hitResultAudio = HitResult.AUDIO(url, titleString)
        title = hitResultAudio.getLink()
        assertEquals(url, title)
    }

    @Test
    fun `Get link for image, video, audio gets URL if title is null`() {
        val titleString = null
        val url = "https://www.mozilla.org"

        val hitResultImage = HitResult.IMAGE(url, titleString)
        var title = hitResultImage.getLink()
        assertEquals(url, title)

        val hitResultVideo = HitResult.VIDEO(url, titleString)
        title = hitResultVideo.getLink()
        assertEquals(url, title)

        val hitResultAudio = HitResult.AUDIO(url, titleString)
        title = hitResultAudio.getLink()
        assertEquals(url, title)
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
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any(), anyBoolean())
        sessionManager.add(Session("https://www.mozilla.org", private = true))

        shareLink.action.invoke(
            store.state.tabs.first(),
            HitResult.IMAGE_SRC("https://firefox.com", "https://getpocket.com"))

        verify(context).startActivity(any())
    }

    @Test
    fun `Candidate "Share image"`() {
        val context = spy(testContext)

        val shareImage = ContextMenuCandidate.createShareImageCandidate(context)

        // showFor

        assertTrue(shareImage.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.IMAGE("https://www.mozilla.org")))

        assertTrue(shareImage.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.IMAGE_SRC("https://www.mozilla.org", "https://www.mozilla.org")))

        assertFalse(shareImage.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.AUDIO("https://www.mozilla.org")))

        // action

        val store = BrowserStore(initialState = BrowserState(
            tabs = listOf(TabSessionState("123", ContentState("https://www.mozilla.org")))
        ))

        shareImage.action.invoke(
            store.state.tabs.first(),
            HitResult.IMAGE_SRC("https://firefox.com", "https://getpocket.com")
        )

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
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any(), anyBoolean())
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
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any(), anyBoolean())
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

    @Test
    fun `Candidate "Open in external app"`() {
        val getAppLinkRedirectMock: AppLinksUseCases.GetAppLinkRedirect = mock()

        doReturn(
            AppLinkRedirect(mock(), null, null)
        ).`when`(getAppLinkRedirectMock).invoke(eq("https://www.example.com"))

        doReturn(
            AppLinkRedirect(null, null, mock())
        ).`when`(getAppLinkRedirectMock).invoke(eq("intent:www.example.com#Intent;scheme=https;package=org.mozilla.fenix;end"))

        doReturn(
            AppLinkRedirect(null, null, null)
        ).`when`(getAppLinkRedirectMock).invoke(eq("https://www.otherexample.com"))

        // This mock exists only to verify that it was called
        val openAppLinkRedirectMock: AppLinksUseCases.OpenAppLinkRedirect = mock()

        val appLinksUseCasesMock: AppLinksUseCases = mock()
        doReturn(getAppLinkRedirectMock).`when`(appLinksUseCasesMock).appLinkRedirectIncludeInstall
        doReturn(openAppLinkRedirectMock).`when`(appLinksUseCasesMock).openAppLink

        val openLinkInExternalApp = ContextMenuCandidate.createOpenInExternalAppCandidate(
            testContext, appLinksUseCasesMock
        )

        // showFor

        assertTrue(openLinkInExternalApp.showFor(
            mock(),
            HitResult.UNKNOWN("https://www.example.com")
        ))

        assertTrue(openLinkInExternalApp.showFor(
            mock(),
            HitResult.UNKNOWN("intent:www.example.com#Intent;scheme=https;package=org.mozilla.fenix;end")
        ))

        assertTrue(openLinkInExternalApp.showFor(
            mock(),
            HitResult.VIDEO("https://www.example.com")
        ))

        assertTrue(openLinkInExternalApp.showFor(
            mock(),
            HitResult.AUDIO("https://www.example.com")
        ))

        assertFalse(openLinkInExternalApp.showFor(
            mock(),
            HitResult.UNKNOWN("https://www.otherexample.com")
        ))

        assertFalse(openLinkInExternalApp.showFor(
            mock(),
            HitResult.VIDEO("https://www.otherexample.com")
        ))

        assertFalse(openLinkInExternalApp.showFor(
            mock(),
            HitResult.AUDIO("https://www.otherexample.com")
        ))

        // action

        openLinkInExternalApp.action.invoke(
            mock(),
            HitResult.UNKNOWN("https://www.example.com"))

        openLinkInExternalApp.action.invoke(
            mock(),
            HitResult.UNKNOWN("intent:www.example.com#Intent;scheme=https;package=org.mozilla.fenix;end"))

        openLinkInExternalApp.action.invoke(
            mock(),
            HitResult.UNKNOWN("https://www.otherexample.com"))

        verify(openAppLinkRedirectMock, times(2)).invoke(any(), anyBoolean(), any())
    }

    @Test
    fun `Candidate "Copy email address"`() {
        val parentView = CoordinatorLayout(testContext)

        val copyEmailAddress = ContextMenuCandidate.createCopyEmailAddressCandidate(
            testContext, parentView, snackbarDelegate)

        // showFor

        assertTrue(copyEmailAddress.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.UNKNOWN("mailto:example@example.com")))

        assertTrue(copyEmailAddress.showFor(
            createTab("https://www.mozilla.org", private = true),
            HitResult.UNKNOWN("mailto:example.com")))

        assertFalse(copyEmailAddress.showFor(
            createTab("https://www.mozilla.org", private = true),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertFalse(copyEmailAddress.showFor(
            createTab("https://www.mozilla.org", private = true),
            HitResult.UNKNOWN("example@example.com")))

        // action

        val store = BrowserStore()
        val sessionManager = spy(SessionManager(mock(), store))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any(), anyBoolean())
        sessionManager.add(Session("https://www.mozilla.org", private = true))

        copyEmailAddress.action.invoke(
            store.state.tabs.first(),
            HitResult.UNKNOWN("mailto:example@example.com"))

        assertTrue(snackbarDelegate.hasShownSnackbar)

        val clipboardManager = testContext.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        assertEquals(
            "example@example.com",
            clipboardManager.primaryClip!!.getItemAt(0).text
        )
    }

    @Test
    fun `Candidate "Share email address"`() {
        val context = spy(testContext)

        val shareEmailAddress = ContextMenuCandidate.createShareEmailAddressCandidate(context)

        // showFor

        assertTrue(shareEmailAddress.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.UNKNOWN("mailto:example@example.com")))

        assertTrue(shareEmailAddress.showFor(
            createTab("https://www.mozilla.org", private = true),
            HitResult.UNKNOWN("mailto:example.com")))

        assertFalse(shareEmailAddress.showFor(
            createTab("https://www.mozilla.org", private = true),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertFalse(shareEmailAddress.showFor(
            createTab("https://www.mozilla.org", private = true),
            HitResult.UNKNOWN("example@example.com")))

        // action

        val store = BrowserStore()
        val sessionManager = spy(SessionManager(mock(), store))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any(), anyBoolean())
        sessionManager.add(Session("https://www.mozilla.org", private = true))

        shareEmailAddress.action.invoke(
            store.state.tabs.first(),
            HitResult.UNKNOWN("mailto:example@example.com"))

        verify(context).startActivity(any())
    }

    @Test
    fun `Candidate "Add to contacts"`() {
        val context = spy(testContext)

        val addToContacts = ContextMenuCandidate.createAddContactCandidate(context)

        // showFor

        assertTrue(addToContacts.showFor(
            createTab("https://www.mozilla.org"),
            HitResult.UNKNOWN("mailto:example@example.com")))

        assertTrue(addToContacts.showFor(
            createTab("https://www.mozilla.org", private = true),
            HitResult.UNKNOWN("mailto:example.com")))

        assertFalse(addToContacts.showFor(
            createTab("https://www.mozilla.org", private = true),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertFalse(addToContacts.showFor(
            createTab("https://www.mozilla.org", private = true),
            HitResult.UNKNOWN("example@example.com")))

        // action

        val store = BrowserStore()
        val sessionManager = spy(SessionManager(mock(), store))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any(), anyBoolean())
        sessionManager.add(Session("https://www.mozilla.org", private = true))

        addToContacts.action.invoke(
            store.state.tabs.first(),
            HitResult.UNKNOWN("mailto:example@example.com"))

        verify(context).startActivity(any())
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
