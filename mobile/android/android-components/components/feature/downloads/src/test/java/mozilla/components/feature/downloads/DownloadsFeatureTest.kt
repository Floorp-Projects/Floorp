/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.Manifest.permission.INTERNET
import android.Manifest.permission.WRITE_EXTERNAL_STORAGE
import junit.framework.Assert.assertTrue
import mozilla.components.browser.session.Download
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.support.base.observer.Consumable
import mozilla.components.support.test.any
import mozilla.components.support.test.robolectric.grantPermission
import mozilla.components.support.test.mock
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
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
        val engine = Mockito.mock(Engine::class.java)
        val context = RuntimeEnvironment.application
        mockSessionManager = spy(SessionManager(engine))
        mockDownloadManager = mock()
        feature = DownloadsFeature(context, downloadManager = mockDownloadManager, sessionManager = mockSessionManager)
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
        val session = Session("https://mozilla.com")
        val download = mock<Download>()

        var needToRequestPermissionCalled = false

        feature.onNeedToRequestPermissions = { _, _ ->
            needToRequestPermissionCalled = true
        }

        feature.start()
        mockSessionManager.add(session)
        mockSessionManager.select(session)

        session.download = Consumable.from(download)

        verify(mockDownloadManager, times(0)).download(download)
        assertTrue(needToRequestPermissionCalled)
    }

    @Test
    fun `when a download came and permissions aren't granted needToRequestPermissions and after onPermissionsGranted the download must be triggered`() {
        val session = Session("https://mozilla.com")
        val download = mock<Download>()
        var needToRequestPermissionCalled = false

        feature.onNeedToRequestPermissions = { _, _ ->
            needToRequestPermissionCalled = true
        }

        feature.start()
        mockSessionManager.add(session)
        mockSessionManager.select(session)

        session.download = Consumable.from(download)

        verify(mockDownloadManager, times(0)).download(download)
        assertTrue(needToRequestPermissionCalled)

        grantPermissions()

        feature.onPermissionsGranted()
        verify(mockDownloadManager).download(download)
    }

    private fun grantPermissions() {
        grantPermission(INTERNET, WRITE_EXTERNAL_STORAGE)
    }
}