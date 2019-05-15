/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.contextmenu

import android.content.ClipboardManager
import android.content.Context
import androidx.coordinatorlayout.widget.CoordinatorLayout
import android.view.View
import mozilla.components.browser.session.Download
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.HitResult
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class ContextMenuCandidateTest {
    private val context: Context get() = RuntimeEnvironment.application
    private lateinit var snackbarDelegate: TestSnackbarDelegate

    @Before
    fun setUp() {
        snackbarDelegate = TestSnackbarDelegate()
    }

    @Test
    fun `Default candidates sanity check`() {
        val candidates = ContextMenuCandidate.defaultCandidates(context, mock(), mock())
        // Just a sanity check: When changing the list of default candidates be aware that this will affect all
        // consumers of this component using the default list.
        assertEquals(7, candidates.size)
    }

    @Test
    fun `Candidate "Open Link in New Tab"`() {
        val sessionManager = spy(SessionManager(mock()))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any())
        val session = Session("https://www.mozilla.org").apply { sessionManager.add(this) }
        val tabsUseCases = TabsUseCases(sessionManager)
        val parentView = CoordinatorLayout(context)

        val openInNewTab = ContextMenuCandidate.createOpenInNewTabCandidate(
            context, tabsUseCases, parentView, snackbarDelegate)

        // showFor

        assertTrue(openInNewTab.showFor(
            Session("https://www.mozilla.org"),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertFalse(openInNewTab.showFor(
            Session("https://www.mozilla.org", private = true),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertTrue(openInNewTab.showFor(
            Session("https://www.mozilla.org"),
            HitResult.IMAGE_SRC("https://www.mozilla.org", "https://www.mozilla.org")))

        assertFalse(openInNewTab.showFor(
            Session("https://www.mozilla.org"),
            HitResult.IMAGE("https://www.mozilla.org")))

        assertFalse(openInNewTab.showFor(
            Session("https://www.mozilla.org"),
            HitResult.VIDEO("https://www.mozilla.org")))

        // action

        assertEquals(1, sessionManager.size)
        assertFalse(snackbarDelegate.hasShownSnackbar)

        openInNewTab.action.invoke(session, HitResult.UNKNOWN("https://firefox.com"))

        assertEquals(2, sessionManager.size)
        assertTrue(snackbarDelegate.hasShownSnackbar)
        assertNotNull(snackbarDelegate.lastAcionListener)
        assertEquals("https://firefox.com", sessionManager.sessions.get(1).url)

        // Snackbar action

        assertEquals(sessionManager.selectedSession, session)

        snackbarDelegate.lastAcionListener!!.invoke(mock())

        assertNotEquals(sessionManager.selectedSession, session)
    }

    @Test
    fun `Candidate "Open Link in Private Tab"`() {
        val sessionManager = spy(SessionManager(mock()))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any())
        val session = Session("https://www.mozilla.org").apply { sessionManager.add(this) }
        val tabsUseCases = TabsUseCases(sessionManager)
        val parentView = CoordinatorLayout(context)

        val openInPrivateTab = ContextMenuCandidate.createOpenInPrivateTabCandidate(
            context, tabsUseCases, parentView, snackbarDelegate)

        // showFor

        assertTrue(openInPrivateTab.showFor(
            Session("https://www.mozilla.org"),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertTrue(openInPrivateTab.showFor(
            Session("https://www.mozilla.org", private = true),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertTrue(openInPrivateTab.showFor(
            Session("https://www.mozilla.org"),
            HitResult.IMAGE_SRC("https://www.mozilla.org", "https://www.mozilla.org")))

        assertFalse(openInPrivateTab.showFor(
            Session("https://www.mozilla.org"),
            HitResult.IMAGE("https://www.mozilla.org")))

        assertFalse(openInPrivateTab.showFor(
            Session("https://www.mozilla.org"),
            HitResult.VIDEO("https://www.mozilla.org")))

        // action

        assertEquals(1, sessionManager.size)
        assertFalse(snackbarDelegate.hasShownSnackbar)

        openInPrivateTab.action.invoke(session, HitResult.UNKNOWN("https://firefox.com"))

        assertTrue(sessionManager.sessions[1].private)
        assertEquals(2, sessionManager.size)
        assertEquals("https://firefox.com", sessionManager.sessions[1].url)
        assertTrue(snackbarDelegate.hasShownSnackbar)
        assertNotNull(snackbarDelegate.lastAcionListener)

        // Snackbar action

        assertEquals(sessionManager.selectedSession, session)

        snackbarDelegate.lastAcionListener!!.invoke(mock())

        assertNotEquals(sessionManager.selectedSession, session)
    }

    @Test
    fun `Candidate "Open Image in New Tab"`() {
        val sessionManager = spy(SessionManager(mock()))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any())
        val session = Session("https://www.mozilla.org").apply { sessionManager.add(this) }
        val tabsUseCases = TabsUseCases(sessionManager)
        val parentView = CoordinatorLayout(context)

        val openImageInTab = ContextMenuCandidate.createOpenImageInNewTabCandidate(
            context, tabsUseCases, parentView, snackbarDelegate)

        // showFor

        assertFalse(openImageInTab.showFor(
            Session("https://www.mozilla.org"),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertFalse(openImageInTab.showFor(
            Session("https://www.mozilla.org", private = true),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertTrue(openImageInTab.showFor(
            Session("https://www.mozilla.org"),
            HitResult.IMAGE_SRC("https://www.mozilla.org", "https://www.mozilla.org")))

        assertTrue(openImageInTab.showFor(
            Session("https://www.mozilla.org"),
            HitResult.IMAGE("https://www.mozilla.org")))

        assertFalse(openImageInTab.showFor(
            Session("https://www.mozilla.org"),
            HitResult.VIDEO("https://www.mozilla.org")))

        // action

        assertEquals(1, sessionManager.size)
        assertFalse(snackbarDelegate.hasShownSnackbar)

        openImageInTab.action.invoke(session, HitResult.IMAGE_SRC("https://firefox.com", "https://getpocket.com"))

        assertEquals(2, sessionManager.size)
        assertFalse(sessionManager.sessions[1].private)
        assertEquals("https://firefox.com", sessionManager.sessions[1].url)
        assertTrue(snackbarDelegate.hasShownSnackbar)
        assertNotNull(snackbarDelegate.lastAcionListener)

        // Snackbar action

        assertEquals(sessionManager.selectedSession, session)

        snackbarDelegate.lastAcionListener!!.invoke(mock())

        assertNotEquals(sessionManager.selectedSession, session)
    }

    @Test
    fun `Candidate "Open Image in New Tab" opens in private tab if session is private`() {
        val sessionManager = spy(SessionManager(mock()))
        doReturn(mock<EngineSession>()).`when`(sessionManager).getOrCreateEngineSession(any())
        val session = Session(
            "https://www.mozilla.org",
            private = true
        ).apply { sessionManager.add(this) }

        val tabsUseCases = TabsUseCases(sessionManager)
        val parentView = CoordinatorLayout(context)

        val openImageInTab = ContextMenuCandidate.createOpenImageInNewTabCandidate(
            context, tabsUseCases, parentView, snackbarDelegate)

        assertEquals(1, sessionManager.size)

        openImageInTab.action.invoke(session, HitResult.IMAGE_SRC("https://firefox.com", "https://getpocket.com"))

        assertEquals(2, sessionManager.size)
        assertTrue(sessionManager.sessions[1].private)
        assertEquals("https://firefox.com", sessionManager.sessions[1].url)
    }

    @Test
    fun `Candidate "Save image"`() {
        val saveImage = ContextMenuCandidate.createSaveImageCandidate(context)

        // showFor

        assertFalse(saveImage.showFor(
            Session("https://www.mozilla.org"),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertFalse(saveImage.showFor(
            Session("https://www.mozilla.org", private = true),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertTrue(saveImage.showFor(
            Session("https://www.mozilla.org"),
            HitResult.IMAGE_SRC("https://www.mozilla.org", "https://www.mozilla.org")))

        assertTrue(saveImage.showFor(
            Session("https://www.mozilla.org"),
            HitResult.IMAGE("https://www.mozilla.org")))

        assertFalse(saveImage.showFor(
            Session("https://www.mozilla.org"),
            HitResult.VIDEO("https://www.mozilla.org")))

        // action

        val session = Session("https://www.mozilla.org")

        assertTrue(session.download.isConsumed())

        saveImage.action.invoke(session,
            HitResult.IMAGE_SRC("https://www.mozilla.org/media/img/logos/firefox/logo-quantum.9c5e96634f92.png",
                "https://firefox.com"))

        assertFalse(session.download.isConsumed())

        var imageDownload: Download? = null
        session.download.consume { download ->
            imageDownload = download
            true
        }

        assertEquals(
            "https://www.mozilla.org/media/img/logos/firefox/logo-quantum.9c5e96634f92.png",
            imageDownload!!.url)
    }

    @Test
    fun `Candidate "Share Link"`() {
        val context = spy(context)

        val shareLink = ContextMenuCandidate.createShareLinkCandidate(context)

        // showFor

        assertTrue(shareLink.showFor(
            Session("https://www.mozilla.org"),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertTrue(shareLink.showFor(
            Session("https://www.mozilla.org", private = true),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertTrue(shareLink.showFor(
            Session("https://www.mozilla.org"),
            HitResult.IMAGE_SRC("https://www.mozilla.org", "https://www.mozilla.org")))

        assertFalse(shareLink.showFor(
            Session("https://www.mozilla.org"),
            HitResult.IMAGE("https://www.mozilla.org")))

        assertFalse(shareLink.showFor(
            Session("https://www.mozilla.org"),
            HitResult.VIDEO("https://www.mozilla.org")))

        // action

        val session = Session("https://www.mozilla.org")

        shareLink.action.invoke(session, HitResult.IMAGE_SRC("https://firefox.com", "https://getpocket.com"))

        verify(context).startActivity(any())
    }

    @Test
    fun `Candidate "Copy Link"`() {
        val parentView = CoordinatorLayout(context)

        val copyLink = ContextMenuCandidate.createCopyLinkCandidate(
            context, parentView, snackbarDelegate)

        // showFor

        assertTrue(copyLink.showFor(
            Session("https://www.mozilla.org"),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertTrue(copyLink.showFor(
            Session("https://www.mozilla.org", private = true),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertTrue(copyLink.showFor(
            Session("https://www.mozilla.org"),
            HitResult.IMAGE_SRC("https://www.mozilla.org", "https://www.mozilla.org")))

        assertFalse(copyLink.showFor(
            Session("https://www.mozilla.org"),
            HitResult.IMAGE("https://www.mozilla.org")))

        assertFalse(copyLink.showFor(
            Session("https://www.mozilla.org"),
            HitResult.VIDEO("https://www.mozilla.org")))

        // action

        val session = Session("https://www.mozilla.org")

        copyLink.action.invoke(session, HitResult.IMAGE_SRC("https://firefox.com", "https://getpocket.com"))

        assertTrue(snackbarDelegate.hasShownSnackbar)

        val clipboardManager = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        assertEquals(
            "https://getpocket.com",
            clipboardManager.primaryClip!!.getItemAt(0).text
        )
    }

    @Test
    fun `Candidate "Copy Image Location"`() {
        val parentView = CoordinatorLayout(context)

        val copyImageLocation = ContextMenuCandidate.createCopyImageLocationCandidate(
            context, parentView, snackbarDelegate)

        // showFor

        assertFalse(copyImageLocation.showFor(
            Session("https://www.mozilla.org"),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertFalse(copyImageLocation.showFor(
            Session("https://www.mozilla.org", private = true),
            HitResult.UNKNOWN("https://www.mozilla.org")))

        assertTrue(copyImageLocation.showFor(
            Session("https://www.mozilla.org"),
            HitResult.IMAGE_SRC("https://www.mozilla.org", "https://www.mozilla.org")))

        assertTrue(copyImageLocation.showFor(
            Session("https://www.mozilla.org"),
            HitResult.IMAGE("https://www.mozilla.org")))

        assertFalse(copyImageLocation.showFor(
            Session("https://www.mozilla.org"),
            HitResult.VIDEO("https://www.mozilla.org")))

        // action

        val session = Session("https://www.mozilla.org")

        copyImageLocation.action.invoke(session, HitResult.IMAGE_SRC("https://firefox.com", "https://getpocket.com"))

        assertTrue(snackbarDelegate.hasShownSnackbar)

        val clipboardManager = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        assertEquals(
            "https://firefox.com",
            clipboardManager.primaryClip!!.getItemAt(0).text
        )
    }
}

private class TestSnackbarDelegate : ContextMenuCandidate.SnackbarDelegate {
    var hasShownSnackbar = false
    var lastAcionListener: ((v: View) -> Unit)? = null

    override fun show(snackBarParentView: View, text: Int, duration: Int, action: Int, listener: ((v: View) -> Unit)?) {
        hasShownSnackbar = true
        lastAcionListener = listener
    }
}