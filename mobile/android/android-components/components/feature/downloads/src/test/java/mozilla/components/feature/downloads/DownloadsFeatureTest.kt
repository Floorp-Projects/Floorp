/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.Manifest.permission.INTERNET
import android.Manifest.permission.WRITE_EXTERNAL_STORAGE
import androidx.core.content.PermissionChecker
import androidx.fragment.app.FragmentManager
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.session.Download
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.downloads.DownloadDialogFragment.Companion.FRAGMENT_TAG
import mozilla.components.feature.downloads.manager.DownloadManager
import mozilla.components.support.base.observer.Consumable
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.grantPermission
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions

@RunWith(AndroidJUnit4::class)
class DownloadsFeatureTest {

    private lateinit var feature: DownloadsFeature
    private lateinit var mockDownloadManager: DownloadManager
    private lateinit var mockSessionManager: SessionManager

    @Before
    fun setup() {
        val engine = mock<Engine>()
        mockSessionManager = spy(SessionManager(engine))
        mockDownloadManager = mock()
        `when`(mockDownloadManager.permissions).thenReturn(arrayOf(INTERNET, WRITE_EXTERNAL_STORAGE))
        feature = DownloadsFeature(testContext,
            downloadManager = mockDownloadManager,
            sessionManager = mockSessionManager)
    }

    @Test
    fun `when valid sessionId is provided, observe it's session`() {
        feature = spy(DownloadsFeature(
            testContext,
            downloadManager = mockDownloadManager,
            sessionManager = mockSessionManager,
            sessionId = "123"
        ))
        `when`(mockSessionManager.findSessionById(anyString())).thenReturn(mock())

        feature.start()

        verify(feature).observeIdOrSelected(anyString())
        verify(feature).observeFixed(any())
    }

    @Test
    fun `when sessionId is NOT provided, observe selected session`() {
        feature = spy(DownloadsFeature(
            testContext,
            downloadManager = mockDownloadManager,
            sessionManager = mockSessionManager
        ))

        feature.start()

        verify(feature).observeIdOrSelected(null)
        verify(feature).observeSelected()
    }

    @Test
    fun `when start is called must register SessionManager observers `() {
        feature.start()
        verify(mockSessionManager).register(feature)
    }

    @Test
    fun `when stop is called must unregister SessionManager observers `() {
        feature.stop()
        verify(mockSessionManager).unregister(feature)
    }

    @Test
    fun `when a download is notify must pass it to the download manager`() {
        val session = Session("https://mozilla.com")
        val download = mock<Download>()
        grantPermissions()

        mockSessionManager.add(session)
        mockSessionManager.select(session)
        feature.start()

        session.download = Consumable.from(download)

        verify(mockDownloadManager).download(download)
    }

    @Test
    fun `when new session is select must register an observer on it `() {
        val session = mock<Session>()
        val download = mock<Download>()

        feature.start()
        session.notifyObservers {
            onDownload(session, download)
        }
        with(mockSessionManager) {
            add(session)
            select(session)
        }

        verify(session, times(2)).register(any())
    }

    @Test
    fun `after stop is called, new download must not pass to the download manager `() {
        val session = Session("https://mozilla.com")
        val download = mock<Download>()

        grantPermissions()

        feature.start()
        mockSessionManager.add(session)
        mockSessionManager.select(session)

        verify(mockDownloadManager).onDownloadCompleted = any()

        session.notifyObservers {
            onDownload(session, download)
        }

        verify(mockDownloadManager).permissions
        verify(mockDownloadManager).download(download)
        feature.stop()

        verify(mockDownloadManager).unregisterListeners()

        session.download = Consumable.from(download)

        verifyNoMoreInteractions(mockDownloadManager)
    }

    @Test
    fun `when a download came and permissions aren't granted needToRequestPermissions must be called `() {
        var needToRequestPermissionCalled = false

        feature.onNeedToRequestPermissions = { needToRequestPermissionCalled = true }

        feature.start()
        val download = startDownload()

        verify(mockDownloadManager, never()).download(download)
        assertTrue(needToRequestPermissionCalled)
    }

    @Test
    fun `when a downloading & permissions aren't granted needToRequestPermissions, after onPermissionsGranted the download must be triggered`() {
        var needToRequestPermissionCalled = false

        feature.onNeedToRequestPermissions = { needToRequestPermissionCalled = true }

        feature.start()

        val download = startDownload()

        verify(mockDownloadManager, times(0)).download(download)
        assertTrue(needToRequestPermissionCalled)

        grantPermissions()

        feature.onPermissionsResult(arrayOf(INTERNET, WRITE_EXTERNAL_STORAGE),
                intArrayOf(PermissionChecker.PERMISSION_GRANTED, PermissionChecker.PERMISSION_GRANTED))
        verify(mockDownloadManager).download(download)
    }

    @Test
    fun `when fragmentManager is not null a confirmation dialog must be show before starting a download`() {
        val mockDialog = spy(DownloadDialogFragment::class.java)
        val mockFragmentManager = mock<FragmentManager>()

        `when`(mockFragmentManager.beginTransaction()).thenReturn(mock())

        val featureWithDialog =
            DownloadsFeature(
                testContext,
                downloadManager = mockDownloadManager,
                sessionManager = mockSessionManager,
                fragmentManager = mockFragmentManager,
                dialog = mockDialog
            )

        grantPermissions()

        featureWithDialog.start()

        val download = startDownload()

        verify(mockDialog).show(mockFragmentManager, FRAGMENT_TAG)
        mockDialog.onStartDownload()
        verify(mockDownloadManager).download(download)
    }

    @Test
    fun `feature with a sessionId will re-attach to already existing fragment`() {
        val mockDialog = spy(DownloadDialogFragment::class.java)
        val mockFragmentManager = mock<FragmentManager>()
        val mockDownload: Consumable<Download> = mock()
        val mockSession: Session = mock()

        doReturn(mockDownload).`when`(mockSession).download
        doReturn("sessionId").`when`(mockSession).id
        doReturn(mockDialog).`when`(mockFragmentManager).findFragmentByTag(any())

        mockSessionManager.add(mockSession)

        val feature =
            DownloadsFeature(
                testContext,
                downloadManager = mockDownloadManager,
                sessionId = "sessionId",
                sessionManager = mockSessionManager,
                fragmentManager = mockFragmentManager
            )

        feature.start()
        assertTrue(feature.dialog !is SimpleDownloadDialogFragment)
        verify(mockSession).download
    }

    @Test
    fun `feature with a selected session will re-attach to already existing fragment`() {
        val mockDialog = spy(DownloadDialogFragment::class.java)
        val mockFragmentManager = mock<FragmentManager>()
        val mockDownload: Consumable<Download> = mock()
        val mockSession: Session = mock()

        doReturn(mockDownload).`when`(mockSession).download
        doReturn("sessionId").`when`(mockSession).id
        doReturn(mockDialog).`when`(mockFragmentManager).findFragmentByTag(any())

        mockSessionManager.add(mockSession)
        mockSessionManager.select(mockSession)

        val feature =
            DownloadsFeature(
                testContext,
                downloadManager = mockDownloadManager,
                sessionId = "sessionId",
                sessionManager = mockSessionManager,
                fragmentManager = mockFragmentManager
            )

        feature.start()
        assertTrue(feature.dialog !is SimpleDownloadDialogFragment)
        verify(mockSession).download
    }

    @Test
    fun `shouldn't show twice a dialog if is already created`() {
        val mockDialog = spy(DownloadDialogFragment::class.java)
        val mockFragmentManager = mock<FragmentManager>()

        `when`(mockFragmentManager.findFragmentByTag(FRAGMENT_TAG)).thenReturn(mockDialog)

        val featureWithDialog =
            DownloadsFeature(
                testContext,
                downloadManager = mockDownloadManager,
                sessionManager = mockSessionManager,
                fragmentManager = mockFragmentManager
            )

        grantPermissions()

        featureWithDialog.start()

        startDownload()

        verify(mockDialog, times(0)).show(mockFragmentManager, FRAGMENT_TAG)
    }

    @Test
    fun `download is cleared when permissions denied`() {
        feature.start()
        feature.onPermissionsResult(arrayOf(INTERNET, WRITE_EXTERNAL_STORAGE),
                intArrayOf(PermissionChecker.PERMISSION_GRANTED, PermissionChecker.PERMISSION_GRANTED))
        assertNull(mockSessionManager.selectedSession)

        val download = startDownload()
        feature.onPermissionsResult(arrayOf(INTERNET, WRITE_EXTERNAL_STORAGE),
                intArrayOf(PermissionChecker.PERMISSION_GRANTED, PermissionChecker.PERMISSION_DENIED))

        verify(mockDownloadManager, never()).download(download)
        assertNotNull(mockSessionManager.selectedSession)
        assertTrue(mockSessionManager.selectedSession!!.download.isConsumed())
    }

    private fun startDownload(): Download {
        val session = Session("https://mozilla.com")
        val download = mock<Download>()

        mockSessionManager.add(session)
        mockSessionManager.select(session)
        session.download = Consumable.from(download)
        return download
    }

    private fun grantPermissions() {
        grantPermission(INTERNET, WRITE_EXTERNAL_STORAGE)
    }
}
