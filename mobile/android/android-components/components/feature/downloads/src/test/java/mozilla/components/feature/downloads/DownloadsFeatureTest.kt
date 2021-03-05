/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.Manifest.permission.INTERNET
import android.Manifest.permission.WRITE_EXTERNAL_STORAGE
import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.content.pm.ActivityInfo
import android.content.pm.PackageManager
import android.content.pm.ResolveInfo
import androidx.fragment.app.FragmentManager
import androidx.fragment.app.FragmentTransaction
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.downloads.DownloadsUseCases.CancelDownloadRequestUseCase
import mozilla.components.feature.downloads.DownloadsUseCases.ConsumeDownloadUseCase
import mozilla.components.feature.downloads.manager.DownloadManager
import mozilla.components.feature.downloads.ui.DownloadAppChooserDialog
import mozilla.components.feature.downloads.ui.DownloaderApp
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.grantPermission
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.doThrow
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
        feature.tryAgain("0")

        verify(downloadManager).tryAgain("0")
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

        doReturn("id").`when`(downloadManager).download(eq(download), anyString())

        store.dispatch(ContentAction.UpdateDownloadAction("test-tab", download))
            .joinBlocking()

        testDispatcher.advanceUntilIdle()
        store.waitUntilIdle()

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
        feature.showDownloadDialog(tab!!, download)

        verify(dialogFragment).onStartDownload = any()
        verify(dialogFragment).onCancelDownload = any()
        verify(dialogFragment).setDownload(download)
        verify(dialogFragment, never()).showNow(any(), any())
    }

    @Test
    fun `WHEN dismissing a download dialog THEN the download stream should be closed`() {
        val downloadsUseCases = spy(DownloadsUseCases(store))
        val closeDownloadResponseUseCase = mock<CancelDownloadRequestUseCase>()
        val download = DownloadState(url = "https://www.mozilla.org", sessionId = "test-tab")
        val dialogFragment = spy(object : DownloadDialogFragment() {})
        val fragmentManager: FragmentManager = mock()

        doReturn(dialogFragment).`when`(fragmentManager).findFragmentByTag(DownloadDialogFragment.FRAGMENT_TAG)
        store.dispatch(ContentAction.UpdateDownloadAction("test-tab", download))
                .joinBlocking()
        doReturn(closeDownloadResponseUseCase).`when`(downloadsUseCases).cancelDownloadRequest

        val feature = spy(DownloadsFeature(
            testContext,
            store,
            useCases = downloadsUseCases,
            downloadManager = mock(),
            fragmentManager = fragmentManager
        ))

        val tab = store.state.findTab("test-tab")

        feature.showDownloadDialog(tab!!, download)

        dialogFragment.onCancelDownload()
        verify(closeDownloadResponseUseCase).invoke(anyString(), anyString())
    }

    @Test
    fun `onPermissionsResult will start download if permissions were granted and thirdParty enabled`() {
        val downloadsUseCases = spy(DownloadsUseCases(store))
        val consumeDownloadUseCase = mock<ConsumeDownloadUseCase>()
        val download = DownloadState(url = "https://www.mozilla.org", sessionId = "test-tab")
        val downloadManager: DownloadManager = mock()
        val permissionsArray = arrayOf(INTERNET, WRITE_EXTERNAL_STORAGE)
        val grantedPermissionsArray = arrayOf(PackageManager.PERMISSION_GRANTED, PackageManager.PERMISSION_GRANTED).toIntArray()

        store.dispatch(ContentAction.UpdateDownloadAction("test-tab", download = download))
            .joinBlocking()

        doReturn(permissionsArray).`when`(downloadManager).permissions
        doReturn(consumeDownloadUseCase).`when`(downloadsUseCases).consumeDownload

        val feature = spy(DownloadsFeature(
            testContext,
            store,
            useCases = downloadsUseCases,
            downloadManager = downloadManager,
            shouldForwardToThirdParties = { true }
        ))

        doReturn(false).`when`(feature).startDownload(any())

        grantPermissions()

        feature.onPermissionsResult(permissionsArray, grantedPermissionsArray)

        verify(feature).startDownload(download)
        verify(feature, never()).processDownload(any(), eq(download))
        verify(consumeDownloadUseCase).invoke(anyString(), anyString())
    }

    @Test
    fun `onPermissionsResult will process download if permissions were granted and thirdParty disabled`() {
        val downloadsUseCases = spy(DownloadsUseCases(store))
        val consumeDownloadUseCase = mock<ConsumeDownloadUseCase>()
        val download = DownloadState(url = "https://www.mozilla.org", sessionId = "test-tab")
        val downloadManager: DownloadManager = mock()
        val permissionsArray = arrayOf(INTERNET, WRITE_EXTERNAL_STORAGE)
        val grantedPermissionsArray = arrayOf(PackageManager.PERMISSION_GRANTED, PackageManager.PERMISSION_GRANTED).toIntArray()

        store.dispatch(ContentAction.UpdateDownloadAction("test-tab", download = download))
            .joinBlocking()

        val feature = spy(DownloadsFeature(
            testContext,
            store,
            useCases = downloadsUseCases,
            downloadManager = downloadManager,
            shouldForwardToThirdParties = { false }
        ))

        doReturn(permissionsArray).`when`(downloadManager).permissions
        doReturn(false).`when`(feature).processDownload(any(), any())

        grantPermissions()

        feature.onPermissionsResult(permissionsArray, grantedPermissionsArray)

        verify(feature).processDownload(any(), eq(download))
        verify(feature, never()).startDownload(download)
        verify(consumeDownloadUseCase, never()).invoke(anyString(), anyString())
    }

    @Test
    fun `onPermissionsResult will cancel the download if permissions were not granted`() {
        val closeDownloadResponseUseCase = mock<CancelDownloadRequestUseCase>()
        val store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab")
            ),
            selectedTabId = "test-tab"
        ))
        val downloadsUseCases = spy(DownloadsUseCases(store))

        doReturn(closeDownloadResponseUseCase).`when`(downloadsUseCases).cancelDownloadRequest

        store.dispatch(ContentAction.UpdateDownloadAction(
            "test-tab", DownloadState("https://www.mozilla.org")
        )).joinBlocking()

        val downloadManager: DownloadManager = mock()
        doReturn(
            arrayOf(INTERNET, WRITE_EXTERNAL_STORAGE)
        ).`when`(downloadManager).permissions

        val feature = spy(DownloadsFeature(
            testContext,
            store,
            useCases = downloadsUseCases,
            downloadManager = downloadManager
        ))

        feature.start()

        feature.onPermissionsResult(
            arrayOf(INTERNET, WRITE_EXTERNAL_STORAGE),
            arrayOf(PackageManager.PERMISSION_GRANTED, PackageManager.PERMISSION_DENIED).toIntArray())

        store.waitUntilIdle()

        verify(downloadManager, never()).download(any(), anyString())
        verify(closeDownloadResponseUseCase).invoke(anyString(), anyString())
        verify(feature).showPermissionDeniedDialog()
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

        feature.showDownloadDialog(mock(), mock(), dialog)

        verify(dialog).showNow(fragmentManager, DownloadDialogFragment.FRAGMENT_TAG)
        doReturn(true).`when`(feature).isAlreadyADownloadDialog()

        feature.showDownloadDialog(mock(), mock(), dialog)
        verify(dialog, times(1)).showNow(fragmentManager, DownloadDialogFragment.FRAGMENT_TAG)
    }

    @Test
    fun `download dialog must NOT be shown WHEN the fragmentManager isDestroyed`() {
        val fragmentManager = mockFragmentManager()
        val dialog = mock<DownloadDialogFragment>()
        val feature = spy(DownloadsFeature(
            testContext,
            store,
            useCases = mock(),
            downloadManager = mock(),
            fragmentManager = fragmentManager
        ))

        doReturn(false).`when`(feature).isAlreadyADownloadDialog()
        doReturn(true).`when`(fragmentManager).isDestroyed

        feature.showDownloadDialog(mock(), mock(), dialog)

        verify(dialog, never()).showNow(fragmentManager, DownloadDialogFragment.FRAGMENT_TAG)
    }

    @Test
    fun `app downloader dialog must NOT be shown WHEN the fragmentManager isDestroyed`() {
        val fragmentManager = mockFragmentManager()
        val dialog = mock<DownloadAppChooserDialog>()
        val feature = spy(DownloadsFeature(
            testContext,
            store,
            useCases = mock(),
            downloadManager = mock(),
            fragmentManager = fragmentManager
        ))

        doReturn(false).`when`(feature).isAlreadyADownloadDialog()
        doReturn(true).`when`(fragmentManager).isDestroyed

        feature.showAppDownloaderDialog(mock(), mock(), emptyList(), dialog)

        verify(dialog, never()).showNow(fragmentManager, DownloadDialogFragment.FRAGMENT_TAG)
    }

    @Test
    fun `processDownload only forward downloads when shouldForwardToThirdParties is true`() {
        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val download = DownloadState(url = "https://www.mozilla.org/file.txt", sessionId = "test-tab")
        val downloadManager: DownloadManager = mock()

        grantPermissions()

        val feature = spy(DownloadsFeature(
            testContext,
            store,
            DownloadsUseCases(store),
            downloadManager = downloadManager,
            shouldForwardToThirdParties = { false }
        ))

        doReturn(arrayOf(INTERNET, WRITE_EXTERNAL_STORAGE)).`when`(downloadManager).permissions
        doReturn(false).`when`(feature).startDownload(download)

        feature.processDownload(tab, download)

        verify(feature, never()).showAppDownloaderDialog(any(), any(), any(), any())
    }

    @Test
    fun `processDownload must not forward downloads to third party apps when we are the only app that can handle the download`() {
        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val download = DownloadState(url = "https://www.mozilla.org/file.txt", sessionId = "test-tab")
        val ourApp = mock<DownloaderApp>()

        grantPermissions()

        val downloadManager: DownloadManager = mock()
        doReturn(arrayOf(INTERNET, WRITE_EXTERNAL_STORAGE)).`when`(downloadManager).permissions

        val feature = spy(DownloadsFeature(
            testContext,
            store,
            DownloadsUseCases(store),
            downloadManager = downloadManager,
            shouldForwardToThirdParties = { true }
        ))

        doReturn(false).`when`(feature).startDownload(download)
        doReturn(listOf(ourApp)).`when`(feature).getDownloaderApps(testContext, download)

        feature.processDownload(tab, download)

        verify(feature, times(0)).showAppDownloaderDialog(any(), any(), any(), any())
    }

    @Test
    fun `processDownload MUST forward downloads to third party apps when there are multiple apps that can handle the download`() {
        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val download = DownloadState(url = "https://www.mozilla.org/file.txt", sessionId = "test-tab")
        val ourApp = mock<DownloaderApp>()
        val anotherApp = mock<DownloaderApp>()

        grantPermissions()

        val downloadManager: DownloadManager = mock()
        doReturn(arrayOf(INTERNET, WRITE_EXTERNAL_STORAGE)).`when`(downloadManager).permissions

        val feature = spy(DownloadsFeature(
            testContext,
            store,
            DownloadsUseCases(store),
            downloadManager = downloadManager,
            shouldForwardToThirdParties = { true }
        ))

        doReturn(false).`when`(feature).startDownload(download)
        doNothing().`when`(feature).showAppDownloaderDialog(any(), any(), any(), any())
        doReturn(listOf(ourApp, anotherApp)).`when`(feature).getDownloaderApps(testContext, download)

        feature.processDownload(tab, download)

        verify(feature).showAppDownloaderDialog(any(), any(), any(), any())
    }

    @Test
    fun `when url is data url return only our app as downloader app`() {
        val context = mock<Context>()
        val download = DownloadState(url = "data:", sessionId = "test-tab")
        val app = mock<ResolveInfo>()

        val activityInfo = mock<ActivityInfo>()
        app.activityInfo = activityInfo
        val nonLocalizedLabel = "nonLocalizedLabel"
        val packageName = "packageName"
        val appName = "Fenix"

        activityInfo.packageName = packageName
        activityInfo.name = appName
        activityInfo.exported = true

        val packageManager = mock<PackageManager>()
        whenever(context.packageManager).thenReturn(packageManager)
        whenever(context.packageName).thenReturn(packageName)
        whenever(app.loadLabel(packageManager)).thenReturn(nonLocalizedLabel)

        val ourApp = DownloaderApp(
            nonLocalizedLabel,
            app,
            packageName,
            appName,
            download.url,
            download.contentType
        )

        val mockList = listOf(app)
        whenever(packageManager.queryIntentActivities(any<Intent>(), anyInt())).thenReturn(mockList)

        val downloadManager: DownloadManager = mock()

        val feature = DownloadsFeature(
            context,
            store,
            DownloadsUseCases(store),
            downloadManager = downloadManager,
            shouldForwardToThirdParties = { true }
        )

        val appList = feature.getDownloaderApps(context, download)

        assertTrue(download.url.startsWith("data:"))
        assertEquals(1, appList.size)
        assertEquals(ourApp, appList[0])
    }

    @Test
    fun `showAppDownloaderDialog MUST setup and show the dialog`() {
        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val download = DownloadState(url = "https://www.mozilla.org/file.txt", sessionId = "test-tab")
        val ourApp = mock<DownloaderApp>()
        val anotherApp = mock<DownloaderApp>()
        val apps = listOf(ourApp, anotherApp)
        val dialog = mock<DownloadAppChooserDialog>()
        val fragmentManager: FragmentManager = mockFragmentManager()
        val feature = spy(DownloadsFeature(
            testContext,
            store,
            DownloadsUseCases(store),
            downloadManager = mock(),
            shouldForwardToThirdParties = { true },
            fragmentManager = fragmentManager
        ))

        feature.showAppDownloaderDialog(tab, download, apps, dialog)

        verify(dialog).setApps(apps)
        verify(dialog).onAppSelected = any()
        verify(dialog).onDismiss = any()
        verify(dialog).showNow(fragmentManager, DownloadAppChooserDialog.FRAGMENT_TAG)
    }

    @Test
    fun `WHEN dismissing a downloader app dialog THEN the download should be canceled`() {
        val downloadsUseCases = spy(DownloadsUseCases(store))
        val cancelDownloadRequestUseCase = mock<CancelDownloadRequestUseCase>()
        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val download = DownloadState(url = "https://www.mozilla.org/file.txt", sessionId = "test-tab")
        val ourApp = mock<DownloaderApp>()
        val anotherApp = mock<DownloaderApp>()
        val apps = listOf(ourApp, anotherApp)
        val dialog = spy(DownloadAppChooserDialog())
        val fragmentManager: FragmentManager = mockFragmentManager()
        val feature = spy(DownloadsFeature(
            testContext,
            store,
            downloadsUseCases,
            downloadManager = mock(),
            shouldForwardToThirdParties = { true },
            fragmentManager = fragmentManager
        ))

        doReturn(cancelDownloadRequestUseCase).`when`(downloadsUseCases).cancelDownloadRequest

        feature.showAppDownloaderDialog(tab, download, apps, dialog)
        dialog.onDismiss()

        verify(cancelDownloadRequestUseCase).invoke(anyString(), anyString())
    }

    @Test
    fun `when isAlreadyAppDownloaderDialog we must NOT show the appChooserDialog`() {
        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val download = DownloadState(url = "https://www.mozilla.org/file.txt", sessionId = "test-tab")
        val ourApp = mock<DownloaderApp>()
        val anotherApp = mock<DownloaderApp>()
        val apps = listOf(ourApp, anotherApp)
        val dialog = mock<DownloadAppChooserDialog>()
        val fragmentManager: FragmentManager = mockFragmentManager()
        val feature = spy(DownloadsFeature(
            testContext,
            store,
            DownloadsUseCases(store),
            downloadManager = mock(),
            shouldForwardToThirdParties = { true },
            fragmentManager = fragmentManager
        ))

        doReturn(dialog).`when`(fragmentManager).findFragmentByTag(DownloadAppChooserDialog.FRAGMENT_TAG)
        doReturn(true).`when`(feature).isAlreadyAppDownloaderDialog()

        feature.showAppDownloaderDialog(tab, download, apps)

        verify(dialog).setApps(apps)
        verify(dialog).onAppSelected = any()
        verify(dialog).onDismiss = any()
        verify(dialog, times(0)).showNow(fragmentManager, DownloadAppChooserDialog.FRAGMENT_TAG)
    }

    @Test
    fun `when our app is selected for downloading and permission granted then we should perform the download`() {
        val spyContext = spy(testContext)
        val downloadsUseCases = spy(DownloadsUseCases(store))
        val consumeDownloadUseCase = mock<ConsumeDownloadUseCase>()
        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val download = DownloadState(url = "https://www.mozilla.org/file.txt", sessionId = "test-tab")
        val ourApp = DownloaderApp(name = "app", packageName = testContext.packageName, resolver = mock(), activityName = "", url = "", contentType = null)
        val anotherApp = mock<DownloaderApp>()
        val apps = listOf(ourApp, anotherApp)
        val dialog = DownloadAppChooserDialog()
        val fragmentManager: FragmentManager = mockFragmentManager()
        val downloadManager: DownloadManager = mock()
        val feature = spy(DownloadsFeature(
            testContext,
            store,
            downloadsUseCases,
            downloadManager = downloadManager,
            shouldForwardToThirdParties = { true },
            fragmentManager = fragmentManager
        ))

        grantPermissions()

        doReturn(dialog).`when`(fragmentManager).findFragmentByTag(DownloadAppChooserDialog.FRAGMENT_TAG)
        doReturn(consumeDownloadUseCase).`when`(downloadsUseCases).consumeDownload
        doReturn(false).`when`(feature).startDownload(any())
        doReturn(arrayOf(INTERNET, WRITE_EXTERNAL_STORAGE)).`when`(downloadManager).permissions

        feature.showAppDownloaderDialog(tab, download, apps)
        dialog.onAppSelected(ourApp)

        verify(feature).startDownload(any())
        verify(consumeDownloadUseCase).invoke(anyString(), anyString())
        verify(spyContext, times(0)).startActivity(any())
    }

    @Test
    fun `when an app third party is selected for downloading we MUST forward the download`() {
        val spyContext = spy(testContext)
        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val downloadsUseCases = spy(DownloadsUseCases(store))
        val consumeDownloadUseCase = mock<ConsumeDownloadUseCase>()
        val download = DownloadState(url = "https://www.mozilla.org/file.txt", sessionId = "test-tab")
        val ourApp = DownloaderApp(name = "app", packageName = "thridparty.app", resolver = mock(), activityName = "", url = "", contentType = null)
        val anotherApp = mock<DownloaderApp>()
        val apps = listOf(ourApp, anotherApp)
        val dialog = DownloadAppChooserDialog()
        val fragmentManager: FragmentManager = mockFragmentManager()
        val feature = spy(DownloadsFeature(
            spyContext,
            store,
            downloadsUseCases,
            downloadManager = mock(),
            shouldForwardToThirdParties = { true },
            fragmentManager = fragmentManager
        ))

        doReturn(false).`when`(feature).startDownload(any())
        doReturn(dialog).`when`(fragmentManager).findFragmentByTag(DownloadAppChooserDialog.FRAGMENT_TAG)
        doReturn(consumeDownloadUseCase).`when`(downloadsUseCases).consumeDownload

        feature.showAppDownloaderDialog(tab, download, apps)
        dialog.onAppSelected(ourApp)

        verify(feature, times(0)).startDownload(any())
        verify(consumeDownloadUseCase).invoke(anyString(), anyString())
        verify(spyContext).startActivity(any())
    }

    @Test
    fun `None exception is thrown when unable to open an app third party for downloading`() {
        val spyContext = spy(testContext)
        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val downloadsUseCases = spy(DownloadsUseCases(store))
        val consumeDownloadUseCase = mock<ConsumeDownloadUseCase>()
        val download = DownloadState(url = "https://www.mozilla.org/file.txt", sessionId = "test-tab")
        val ourApp = DownloaderApp(name = "app", packageName = "thridparty.app", resolver = mock(), activityName = "", url = "", contentType = null)
        val anotherApp = mock<DownloaderApp>()
        val apps = listOf(ourApp, anotherApp)
        val dialog = DownloadAppChooserDialog()
        val fragmentManager: FragmentManager = mockFragmentManager()
        val feature = spy(DownloadsFeature(
            spyContext,
            store,
            downloadsUseCases,
            downloadManager = mock(),
            shouldForwardToThirdParties = { true },
            fragmentManager = fragmentManager
        ))

        doThrow(ActivityNotFoundException()).`when`(spyContext).startActivity(any())
        doReturn(false).`when`(feature).startDownload(any())
        doReturn(dialog).`when`(fragmentManager).findFragmentByTag(DownloadAppChooserDialog.FRAGMENT_TAG)
        doReturn(consumeDownloadUseCase).`when`(downloadsUseCases).consumeDownload

        feature.showAppDownloaderDialog(tab, download, apps)
        dialog.onAppSelected(ourApp)

        verify(feature, times(0)).startDownload(any())
        verify(consumeDownloadUseCase).invoke(anyString(), anyString())
        verify(spyContext).startActivity(any())
    }

    @Test
    fun `when the appChooserDialog is dismissed THEN the download must be canceled`() {
        val spyContext = spy(testContext)
        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val downloadsUseCases = spy(DownloadsUseCases(store))
        val cancelDownloadRequestUseCase = mock<CancelDownloadRequestUseCase>()
        val download = DownloadState(url = "https://www.mozilla.org/file.txt", sessionId = "test-tab")
        val ourApp = mock<DownloaderApp>()
        val anotherApp = mock<DownloaderApp>()
        val apps = listOf(ourApp, anotherApp)
        val dialog = DownloadAppChooserDialog()
        val fragmentManager: FragmentManager = mockFragmentManager()
        val feature = spy(DownloadsFeature(
            spyContext,
            store,
            downloadsUseCases,
            downloadManager = mock(),
            shouldForwardToThirdParties = { true },
            fragmentManager = fragmentManager
        ))

        doReturn(false).`when`(feature).startDownload(any())
        doReturn(dialog).`when`(fragmentManager).findFragmentByTag(DownloadAppChooserDialog.FRAGMENT_TAG)
        doReturn(cancelDownloadRequestUseCase).`when`(downloadsUseCases).cancelDownloadRequest

        feature.showAppDownloaderDialog(tab, download, apps)
        dialog.onDismiss()

        verify(cancelDownloadRequestUseCase).invoke(anyString(), anyString())
    }

    @Test
    fun `ResolveInfo to DownloaderApps`() {
        val spyContext = spy(testContext)
        val download = DownloadState(url = "https://www.mozilla.org/file.txt", sessionId = "test-tab")
        val info = ActivityInfo().apply {
            packageName = "thridparty.app"
            name = "activityName"
            icon = android.R.drawable.btn_default
        }
        val resolveInfo = ResolveInfo().apply {
            labelRes = android.R.string.ok
            activityInfo = info
            nonLocalizedLabel = "app"
        }

        val expectedApp = DownloaderApp(name = "app", packageName = "thridparty.app", resolver = resolveInfo, activityName = "activityName", url = download.url, contentType = download.contentType)

        val app = resolveInfo.toDownloaderApp(spyContext, download)
        assertEquals(expectedApp, app)
    }

    @Test
    fun `previous dialogs MUST be dismissed when navigating to another website`() {
        val downloadsUseCases = spy(DownloadsUseCases(store))
        val cancelDownloadRequestUseCase = mock<CancelDownloadRequestUseCase>()
        val download = DownloadState(url = "https://www.mozilla.org", sessionId = "test-tab")
        store.dispatch(ContentAction.UpdateDownloadAction("test-tab", download = download))
                .joinBlocking()

        doReturn(cancelDownloadRequestUseCase).`when`(downloadsUseCases).cancelDownloadRequest

        val feature = spy(DownloadsFeature(
            testContext,
            store,
            useCases = downloadsUseCases,
            downloadManager = mock()
        ))

        doNothing().`when`(feature).dismissAllDownloadDialogs()
        doReturn(true).`when`(feature).processDownload(any(), any())

        feature.start()

        store.dispatch(ContentAction.UpdateDownloadAction("test-tab", download = download))
                .joinBlocking()

        grantPermissions()

        val tab = createTab("https://www.firefox.com")
        store.dispatch(TabListAction.AddTabAction(tab, select = true)).joinBlocking()

        verify(feature).dismissAllDownloadDialogs()
        verify(downloadsUseCases).cancelDownloadRequest
        assertNull(feature.previousTab)
    }

    @Test
    fun `previous dialogs must NOT be dismissed when navigating on the same website`() {
        val downloadsUseCases = spy(DownloadsUseCases(store))
        val cancelDownloadRequestUseCase = mock<CancelDownloadRequestUseCase>()
        val download = DownloadState(url = "https://www.mozilla.org", sessionId = "test-tab")
        store.dispatch(ContentAction.UpdateDownloadAction("test-tab", download = download))
                .joinBlocking()

        doReturn(cancelDownloadRequestUseCase).`when`(downloadsUseCases).cancelDownloadRequest

        val feature = spy(DownloadsFeature(
            testContext,
            store,
            useCases = downloadsUseCases,
            downloadManager = mock()
        ))

        doNothing().`when`(feature).dismissAllDownloadDialogs()
        doReturn(true).`when`(feature).processDownload(any(), any())

        feature.start()

        store.dispatch(ContentAction.UpdateDownloadAction("test-tab", download = download))
                .joinBlocking()

        grantPermissions()

        val tab = createTab("https://www.mozilla.org/example")
        store.dispatch(TabListAction.AddTabAction(tab, select = true)).joinBlocking()

        verify(feature, never()).dismissAllDownloadDialogs()
        verify(downloadsUseCases, never()).cancelDownloadRequest
        assertNotNull(feature.previousTab)
    }

    @Test
    fun `when our app is selected for downloading and permission not granted then we should ask for permission`() {
        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val download = DownloadState(url = "https://www.mozilla.org/file.txt", sessionId = "test-tab")
        val ourApp = DownloaderApp(name = "app", packageName = testContext.packageName, resolver = mock(), activityName = "", url = "", contentType = null)
        val anotherApp = mock<DownloaderApp>()
        val apps = listOf(ourApp, anotherApp)
        val downloadManager: DownloadManager = mock()
        var permissionsRequested = false
        val dialog = DownloadAppChooserDialog()
        val downloadsUseCases = spy(DownloadsUseCases(store))
        val consumeDownloadUseCase = mock<ConsumeDownloadUseCase>()
        val fragmentManager: FragmentManager = mockFragmentManager()

        val feature = spy(DownloadsFeature(
            testContext,
            store,
            useCases = downloadsUseCases,
            downloadManager = downloadManager,
            shouldForwardToThirdParties = { true },
            onNeedToRequestPermissions = { permissionsRequested = true },
            fragmentManager = fragmentManager
        ))

        doReturn(arrayOf(INTERNET, WRITE_EXTERNAL_STORAGE)).`when`(downloadManager).permissions
        doReturn(testContext.packageName).`when`(spy(ourApp)).packageName
        doReturn(dialog).`when`(fragmentManager).findFragmentByTag(DownloadAppChooserDialog.FRAGMENT_TAG)
        doReturn(consumeDownloadUseCase).`when`(downloadsUseCases).consumeDownload

        assertFalse(permissionsRequested)

        feature.showAppDownloaderDialog(tab, download, apps, dialog)
        dialog.onAppSelected(ourApp)

        assertTrue(permissionsRequested)

        verify(feature, never()).startDownload(any())
        verify(spy(testContext), never()).startActivity(any())
        verify(consumeDownloadUseCase, never()).invoke(anyString(), anyString())
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
