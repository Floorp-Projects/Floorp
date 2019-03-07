/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.Manifest.permission.INTERNET
import android.Manifest.permission.WRITE_EXTERNAL_STORAGE
import androidx.core.content.PermissionChecker
import androidx.fragment.app.FragmentManager
import org.junit.Assert.assertTrue
import mozilla.components.browser.session.Download
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.downloads.DownloadDialogFragment.Companion.FRAGMENT_TAG
import mozilla.components.support.base.observer.Consumable
import mozilla.components.support.test.any
import mozilla.components.support.test.robolectric.grantPermission
import mozilla.components.support.test.mock
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions
import org.mockito.Mockito.times
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class DownloadsFeatureTest {

    private lateinit var feature: DownloadsFeature
    private lateinit var mockDownloadManager: DownloadManager
    private lateinit var mockSessionManager: SessionManager

    @Before
    fun setup() {
        val engine = mock(Engine::class.java)
        val context = RuntimeEnvironment.application
        mockSessionManager = spy(SessionManager(engine))
        mockDownloadManager = mock()
        feature = DownloadsFeature(context, downloadManager = mockDownloadManager, sessionManager = mockSessionManager)
    }

    @Test
    fun `when valid sessionId is provided, observe it's session`() {
        feature = spy(DownloadsFeature(
            RuntimeEnvironment.application,
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
            RuntimeEnvironment.application,
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
    fun `after stop is called and new download must not pass the download to the download manager `() {
        val session = Session("https://mozilla.com")
        val download = mock<Download>()

        grantPermissions()

        feature.start()
        mockSessionManager.add(session)
        mockSessionManager.select(session)

        session.notifyObservers {
            onDownload(session, download)
        }

        verify(mockDownloadManager).download(download)
        feature.stop()

        verify(mockDownloadManager).unregisterListener()

        session.download = Consumable.from(download)

        verifyNoMoreInteractions(mockDownloadManager)
    }

    @Test
    fun `when a download came and permissions aren't granted needToRequestPermissions must be called `() {
        var needToRequestPermissionCalled = false

        feature.onNeedToRequestPermissions = { needToRequestPermissionCalled = true }

        feature.start()
        val download = startDownload()

        verify(mockDownloadManager, times(0)).download(download)
        assertTrue(needToRequestPermissionCalled)
    }

    @Test
    fun `when a download came and permissions aren't granted needToRequestPermissions and after onPermissionsGranted the download must be triggered`() {
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
        val context = RuntimeEnvironment.application
        val mockDialog = spy(DownloadDialogFragment::class.java)
        val mockFragmentManager = mock(FragmentManager::class.java)

        `when`(mockFragmentManager.beginTransaction()).thenReturn(mock())

        val featureWithDialog =
            DownloadsFeature(
                context, downloadManager = mockDownloadManager, sessionManager = mockSessionManager,
                fragmentManager = mockFragmentManager, dialog = mockDialog
            )

        grantPermissions()

        featureWithDialog.start()

        val download = startDownload()

        verify(mockDialog).show(mockFragmentManager, FRAGMENT_TAG)
        mockDialog.onStartDownload()
        verify(mockDownloadManager).download(download)
    }

    @Test
    fun `shouldn't show twice a dialog if is already created`() {
        val context = RuntimeEnvironment.application
        val mockDialog = spy(DownloadDialogFragment::class.java)
        val mockFragmentManager = mock(FragmentManager::class.java)

        `when`(mockFragmentManager.findFragmentByTag(FRAGMENT_TAG)).thenReturn(mockDialog)

        val featureWithDialog =
            DownloadsFeature(
                context, downloadManager = mockDownloadManager, sessionManager = mockSessionManager,
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