/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import android.app.Application
import android.content.pm.ActivityInfo
import android.content.pm.PackageManager
import android.content.pm.ResolveInfo
import android.graphics.drawable.Drawable
import android.net.ConnectivityManager
import androidx.core.content.getSystemService
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.runBlocking
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.TestApplication
import org.mozilla.fenix.ext.application
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.isOnline
import org.mozilla.fenix.share.listadapters.AppShareOption
import org.mozilla.fenix.share.listadapters.SyncShareOption
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)
class ShareViewModelTest {

    private val packageName = "org.mozilla.fenix"
    private lateinit var application: Application
    private lateinit var packageManager: PackageManager
    private lateinit var connectivityManager: ConnectivityManager
    private lateinit var fxaAccountManager: FxaAccountManager
    private lateinit var viewModel: ShareViewModel

    @Before
    fun setup() {
        application = spyk(testContext.application)
        packageManager = mockk(relaxed = true)
        connectivityManager = mockk(relaxed = true)
        fxaAccountManager = mockk(relaxed = true)

        mockkStatic("org.mozilla.fenix.ext.ConnectivityManagerKt")

        every { application.packageName } returns packageName
        every { application.packageManager } returns packageManager
        every { application.getSystemService<ConnectivityManager>() } returns connectivityManager
        every { application.components.backgroundServices.accountManager } returns fxaAccountManager

        viewModel = ShareViewModel(application)
    }

    @Test
    fun `liveData should be initialized as empty list`() {
        assertEquals(emptyList<SyncShareOption>(), viewModel.devicesList.value)
        assertEquals(emptyList<AppShareOption>(), viewModel.appsList.value)
    }

    @Test
    fun `loadDevicesAndApps registers networkCallback`() = runBlocking {
        viewModel.loadDevicesAndApps()

        verify { connectivityManager.registerNetworkCallback(any(), eq(viewModel.networkCallback)) }
    }

    @Test
    fun `buildAppsList transforms ResolveInfo list`() {
        assertEquals(emptyList<AppShareOption>(), viewModel.buildAppsList(null, application))

        val icon1: Drawable = mockk()
        val icon2: Drawable = mockk()

        val info = listOf(
            createResolveInfo("App 0", icon1, "package 0", "activity 0"),
            createResolveInfo("Self", mockk(), packageName, "activity self"),
            createResolveInfo("App 1", icon2, "package 1", "activity 1")
        )
        val apps = listOf(
            AppShareOption("App 0", icon1, "package 0", "activity 0"),
            AppShareOption("App 1", icon2, "package 1", "activity 1")
        )
        assertEquals(apps, viewModel.buildAppsList(info, application))
    }

    @Test
    fun `buildDevicesList returns offline option`() {
        every { connectivityManager.isOnline() } returns false
        assertEquals(listOf(SyncShareOption.Offline), viewModel.buildDeviceList(fxaAccountManager))

        every { connectivityManager.getNetworkCapabilities(connectivityManager.activeNetwork) } returns null
        assertEquals(listOf(SyncShareOption.Offline), viewModel.buildDeviceList(fxaAccountManager))
    }

    @Test
    fun `buildDevicesList returns sign-in option`() {
        every { connectivityManager.isOnline() } returns true
        every { fxaAccountManager.authenticatedAccount() } returns null

        assertEquals(listOf(SyncShareOption.SignIn), viewModel.buildDeviceList(fxaAccountManager))
    }

    @Test
    fun `buildDevicesList returns reconnect option`() {
        every { connectivityManager.isOnline() } returns true
        every { fxaAccountManager.authenticatedAccount() } returns mockk()
        every { fxaAccountManager.accountNeedsReauth() } returns true

        assertEquals(listOf(SyncShareOption.Reconnect), viewModel.buildDeviceList(fxaAccountManager))
    }

    private fun createResolveInfo(
        label: String,
        icon: Drawable,
        packageName: String,
        name: String
    ): ResolveInfo {
        val info = ResolveInfo().apply {
            activityInfo = ActivityInfo()
            activityInfo.packageName = packageName
            activityInfo.name = name
        }
        val spy = spyk(info)
        every { spy.loadLabel(packageManager) } returns label
        every { spy.loadIcon(packageManager) } returns icon
        return spy
    }
}
