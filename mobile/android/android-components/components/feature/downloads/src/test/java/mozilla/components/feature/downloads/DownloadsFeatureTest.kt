/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.Manifest.permission.INTERNET
import android.Manifest.permission.WRITE_EXTERNAL_STORAGE
import android.content.pm.PackageManager
import androidx.fragment.app.FragmentManager
import androidx.fragment.app.FragmentTransaction
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.downloads.manager.DownloadManager
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.grantPermission
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.robolectric.shadows.ShadowToast

@RunWith(AndroidJUnit4::class)
class DownloadsFeatureTest {

    private val testDispatcher = TestCoroutineDispatcher()

    private lateinit var store: BrowserStore

    @Before
    @ExperimentalCoroutinesApi
    fun setUp() {
        Dispatchers.setMain(testDispatcher)

        store = BrowserStore(BrowserState(
            tabs = listOf(createTab("https://www.mozilla.org", id = "test-tab")),
            selectedTabId = "test-tab"
        ))
    }

    @After
    @ExperimentalCoroutinesApi
    fun tearDown() {
        Dispatchers.resetMain()
        testDispatcher.cleanupTestCoroutines()
    }

    @Test
    fun `Adding a download object will request permissions if needed`() {
        val fragmentManager: FragmentManager = mock()

        val download = DownloadState(url = "https://www.mozilla.org", sessionId = "test-tab")

        var requestedPermissions = false

        val feature = DownloadsFeature(
            testContext,
            store,
            useCases = mock(),
            onNeedToRequestPermissions = { requestedPermissions = true },
            fragmentManager = mockFragmentManager()
        )

        feature.start()

        assertFalse(requestedPermissions)

        store.dispatch(ContentAction.UpdateDownloadAction("test-tab", download))
            .joinBlocking()

        testDispatcher.advanceUntilIdle()

        assertTrue(requestedPermissions)
        verify(fragmentManager, never()).beginTransaction()
    }

    @Test
    fun `Adding a download when permissions are granted will show dialog`() {
        val fragmentManager: FragmentManager = mockFragmentManager()

        grantPermissions()

        val feature = DownloadsFeature(
            testContext,
            store,
            useCases = mock(),
            fragmentManager = fragmentManager
        )

        feature.start()

        verify(fragmentManager, never()).beginTransaction()
        val download = DownloadState(url = "https://www.mozilla.org", sessionId = "test-tab")

        store.dispatch(ContentAction.UpdateDownloadAction("test-tab", download))
            .joinBlocking()

        testDispatcher.advanceUntilIdle()

        verify(fragmentManager).beginTransaction()
    }

    @Test
    fun `Try again calls download manager`() {
        val fragmentManager: FragmentManager = mockFragmentManager()

        val downloadManager: DownloadManager = mock()

        grantPermissions()

        val feature = DownloadsFeature(
            testContext,
            store,
            useCases = mock(),
            fragmentManager = fragmentManager,
            downloadManager = downloadManager
        )

        feature.start()
        feature.tryAgain(0)

        verify(downloadManager).tryAgain(0)
    }

    @Test
    fun `Adding a download without a fragment manager will start download immediately`() {
        grantPermissions()

        val downloadManager: DownloadManager = mock()
        doReturn(
            arrayOf(INTERNET, WRITE_EXTERNAL_STORAGE)
        ).`when`(downloadManager).permissions

        val feature = DownloadsFeature(
            testContext,
            store,
            useCases = DownloadsUseCases(store),
            downloadManager = downloadManager
        )

        feature.start()

        verify(downloadManager, never()).download(any(), anyString())

        val download = DownloadState(url = "https://www.mozilla.org", sessionId = "test-tab")
        store.dispatch(ContentAction.UpdateDownloadAction("test-tab", download))
            .joinBlocking()

        testDispatcher.advanceUntilIdle()

        verify(downloadManager).download(eq(download), anyString())
    }

    @Test
    fun `Adding a Download with skipConfirmation flag will start download immediately`() {
        val fragmentManager: FragmentManager = mockFragmentManager()

        grantPermissions()

        val downloadManager: DownloadManager = mock()
        doReturn(
            arrayOf(INTERNET, WRITE_EXTERNAL_STORAGE)
        ).`when`(downloadManager).permissions

        val feature = DownloadsFeature(
            testContext,
            store,
            useCases = DownloadsUseCases(store),
            fragmentManager = fragmentManager,
            downloadManager = downloadManager
        )

        feature.start()

        verify(fragmentManager, never()).beginTransaction()

        val download = DownloadState(
            url = "https://www.mozilla.org",
            skipConfirmation = true,
            sessionId = "test-tab"
        )

        store.dispatch(ContentAction.UpdateDownloadAction("test-tab", download))
            .joinBlocking()

        testDispatcher.advanceUntilIdle()

        verify(fragmentManager, never()).beginTransaction()
        verify(downloadManager).download(eq(download), anyString())

        assertNull(store.state.findTab("test-tab")!!.content.download)
    }

    @Test
    fun `When starting a download an existing dialog is reused`() {
        grantPermissions()

        val download = DownloadState(url = "https://www.mozilla.org", sessionId = "test-tab")
        store.dispatch(ContentAction.UpdateDownloadAction("test-tab", download))
            .joinBlocking()

        val dialogFragment: DownloadDialogFragment = mock()
        val fragmentManager: FragmentManager = mock()
        doReturn(dialogFragment).`when`(fragmentManager).findFragmentByTag(DownloadDialogFragment.FRAGMENT_TAG)

        val downloadManager: DownloadManager = mock()
        doReturn(
            arrayOf(INTERNET, WRITE_EXTERNAL_STORAGE)
        ).`when`(downloadManager).permissions

        val feature = DownloadsFeature(
            testContext,
            store,
            useCases = mock(),
            downloadManager = downloadManager,
            fragmentManager = fragmentManager
        )

        val tab = store.state.findTab("test-tab")
        feature.showDialog(tab!!, download)

        verify(dialogFragment).onStartDownload = any()
        verify(dialogFragment).onCancelDownload = any()
        verify(dialogFragment).setDownload(download)
        verify(dialogFragment, never()).showNow(any(), any())
    }

    @Test
    fun `onPermissionsResult will start download if permissions were granted`() {
        val download = DownloadState(url = "https://www.mozilla.org", sessionId = "test-tab")
        store.dispatch(ContentAction.UpdateDownloadAction("test-tab", download = download))
            .joinBlocking()

        val downloadManager: DownloadManager = mock()
        doReturn(
            arrayOf(INTERNET, WRITE_EXTERNAL_STORAGE)
        ).`when`(downloadManager).permissions

        val feature = DownloadsFeature(
            testContext,
            store,
            useCases = DownloadsUseCases(store),
            downloadManager = downloadManager
        )

        feature.start()

        verify(downloadManager, never()).download(download)

        grantPermissions()

        feature.onPermissionsResult(
            arrayOf(INTERNET, WRITE_EXTERNAL_STORAGE),
            arrayOf(
                PackageManager.PERMISSION_GRANTED,
                PackageManager.PERMISSION_GRANTED
            ).toIntArray()
        )

        verify(downloadManager).download(download)
    }

    @Test
    fun `onPermissionsResult will consume download if permissions were not granted`() {
        val store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab")
            ),
            selectedTabId = "test-tab"
        ))

        store.dispatch(ContentAction.UpdateDownloadAction(
            "test-tab", DownloadState("https://www.mozilla.org")
        )).joinBlocking()

        val downloadManager: DownloadManager = mock()
        doReturn(
            arrayOf(INTERNET, WRITE_EXTERNAL_STORAGE)
        ).`when`(downloadManager).permissions

        val feature = DownloadsFeature(
            testContext,
            store,
            useCases = DownloadsUseCases(store),
            downloadManager = downloadManager
        )

        feature.start()

        println(store.state.findTab("test-tab"))
        assertNotNull(store.state.findTab("test-tab")!!.content.download)

        feature.onPermissionsResult(
            arrayOf(INTERNET, WRITE_EXTERNAL_STORAGE),
            arrayOf(PackageManager.PERMISSION_GRANTED, PackageManager.PERMISSION_DENIED).toIntArray())

        store.waitUntilIdle()

        assertNull(store.state.findTab("test-tab")!!.content.download)

        verify(downloadManager, never()).download(any(), anyString())
    }

    @Test
    fun `Calling stop() will unregister listeners from download manager`() {
        val downloadManager: DownloadManager = mock()

        val feature = DownloadsFeature(
            testContext,
            store,
            useCases = mock(),
            downloadManager = downloadManager
        )

        feature.start()

        verify(downloadManager, never()).unregisterListeners()

        feature.stop()

        verify(downloadManager).unregisterListeners()
    }

    @Test
    fun `DownloadManager failing to start download will cause error toast to be displayed`() {
        grantPermissions()

        val downloadManager: DownloadManager = mock()
        doReturn(
            arrayOf(INTERNET, WRITE_EXTERNAL_STORAGE)
        ).`when`(downloadManager).permissions

        doReturn(null).`when`(downloadManager).download(any(), anyString())

        val feature = spy(DownloadsFeature(
            testContext,
            store,
            useCases = DownloadsUseCases(store),
            downloadManager = downloadManager
        ))

        doNothing().`when`(feature).showDownloadNotSupportedError()

        feature.start()

        verify(downloadManager, never()).download(any(), anyString())
        verify(feature, never()).showDownloadNotSupportedError()

        val download = DownloadState(url = "https://www.mozilla.org", sessionId = "test-tab")
        store.dispatch(ContentAction.UpdateDownloadAction("test-tab", download))
            .joinBlocking()

        testDispatcher.advanceUntilIdle()

        verify(downloadManager).download(eq(download), anyString())
        verify(feature).showDownloadNotSupportedError()
    }

    @Test
    fun `showDownloadNotSupportedError shows toast`() {
        // We need to create a Toast on the actual main thread (with a Looper) and therefore reset
        // the main dispatcher that was set to a TestDispatcher in setUp().
        Dispatchers.resetMain()

        grantPermissions()

        val downloadManager: DownloadManager = mock()
        doReturn(
            arrayOf(INTERNET, WRITE_EXTERNAL_STORAGE)
        ).`when`(downloadManager).permissions

        doReturn(null).`when`(downloadManager).download(any(), anyString())

        val feature = spy(DownloadsFeature(
            testContext,
            store,
            useCases = mock(),
            downloadManager = downloadManager
        ))

        feature.showDownloadNotSupportedError()

        val toast = ShadowToast.getTextOfLatestToast()
        assertNotNull(toast)
        assertTrue(toast.contains("can’t download this file type"))
    }

    @Test
    fun `download dialog must be added once`() {
        val fragmentManager = mockFragmentManager()
        val dialog = mock<DownloadDialogFragment>()
        val feature = spy(DownloadsFeature(
            testContext,
            store,
            useCases = mock(),
            downloadManager = mock(),
            fragmentManager = fragmentManager
        ))

        feature.showDialog(mock(), mock(), dialog)

        verify(dialog).showNow(fragmentManager, DownloadDialogFragment.FRAGMENT_TAG)
        doReturn(true).`when`(feature).isAlreadyADialogCreated()

        feature.showDialog(mock(), mock(), dialog)
        verify(dialog, times(1)).showNow(fragmentManager, DownloadDialogFragment.FRAGMENT_TAG)
    }
}

private fun grantPermissions() {
    grantPermission(INTERNET, WRITE_EXTERNAL_STORAGE)
}

private fun mockFragmentManager(): FragmentManager {
    val fragmentManager: FragmentManager = mock()
    doReturn(mock<FragmentTransaction>()).`when`(fragmentManager).beginTransaction()
    return fragmentManager
}
