/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.fenix.translations.preferences.downloadlanguages

import android.net.ConnectivityManager
import android.os.Build
import androidx.core.net.ConnectivityManagerCompat
import androidx.test.ext.junit.runners.AndroidJUnit4
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mozilla.fenix.wifi.WifiConnectionMonitor
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
class DownloadLanguagesFeatureTest {
    private lateinit var downloadLanguagesFeature: DownloadLanguagesFeature
    private lateinit var wifiConnectionMonitor: WifiConnectionMonitor
    private lateinit var dataSaverAndWifiChanged: ((Boolean) -> Unit)
    private lateinit var connectivityManager: ConnectivityManager

    @Before
    fun setUp() {
        wifiConnectionMonitor = mockk(relaxed = true)
        dataSaverAndWifiChanged = mock()
        connectivityManager = mockk()
        downloadLanguagesFeature =
            DownloadLanguagesFeature(
                context = testContext,
                wifiConnectionMonitor = wifiConnectionMonitor,
                onDataSaverAndWifiChanged = dataSaverAndWifiChanged,
            )

        downloadLanguagesFeature.connectivityManager = mockk()
    }

    @Test
    fun `GIVEN fragment is added WHEN the feature starts THEN listen for wifi changes`() {
        downloadLanguagesFeature.start()

        verify(exactly = 1) {
            wifiConnectionMonitor.start()
        }
        verify(exactly = 1) {
            wifiConnectionMonitor.addOnWifiConnectedChangedListener(
                downloadLanguagesFeature.wifiConnectedListener,
            )
        }
        Assert.assertNotNull(downloadLanguagesFeature.connectivityManager)
    }

    @Test
    fun `WHEN stopping the feature THEN all listeners will be removed`() {
        downloadLanguagesFeature.stop()

        verify(exactly = 1) {
            wifiConnectionMonitor.stop()
        }
        verify(exactly = 1) {
            wifiConnectionMonitor.removeOnWifiConnectedChangedListener(
                downloadLanguagesFeature.wifiConnectedListener,
            )
        }
        Assert.assertNull(downloadLanguagesFeature.connectivityManager)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.N])
    fun `GIVEN wifi is connected WHEN wifi changes to not connected and restrictBackgroundStatus is RESTRICT_BACKGROUND_STATUS_ENABLED THEN onDataSaverAndWifiChanged callback should return true`() {
        every { connectivityManager.restrictBackgroundStatus } returns
            ConnectivityManagerCompat.RESTRICT_BACKGROUND_STATUS_ENABLED
        downloadLanguagesFeature.start()
        downloadLanguagesFeature.connectivityManager = connectivityManager

        downloadLanguagesFeature.wifiConnectedListener(false)

        Mockito.verify(dataSaverAndWifiChanged).invoke(true)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.N])
    fun `GIVEN wifi is connected WHEN wifi changes to not connected and restrictBackgroundStatus is RESTRICT_BACKGROUND_STATUS_WHITELISTED THEN onDataSaverAndWifiChanged callback should return true`() {
        every { connectivityManager.restrictBackgroundStatus } returns
            ConnectivityManagerCompat.RESTRICT_BACKGROUND_STATUS_WHITELISTED
        downloadLanguagesFeature.start()
        downloadLanguagesFeature.connectivityManager = connectivityManager

        downloadLanguagesFeature.wifiConnectedListener(false)

        Mockito.verify(dataSaverAndWifiChanged).invoke(true)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.N])
    fun `GIVEN wifi is connected WHEN wifi changes to connected and restrictBackgroundStatus is RESTRICT_BACKGROUND_STATUS_WHITELISTED THEN onDataSaverAndWifiChanged callback should return false`() {
        every { connectivityManager.restrictBackgroundStatus } returns
            ConnectivityManagerCompat.RESTRICT_BACKGROUND_STATUS_WHITELISTED
        downloadLanguagesFeature.start()
        downloadLanguagesFeature.connectivityManager = connectivityManager

        downloadLanguagesFeature.wifiConnectedListener(true)

        Mockito.verify(dataSaverAndWifiChanged).invoke(false)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.N])
    fun `GIVEN wifi is connected WHEN wifi changes to connected and restrictBackgroundStatus is RESTRICT_BACKGROUND_STATUS_DISABLED THEN onDataSaverAndWifiChanged callback should return false`() {
        every { connectivityManager.restrictBackgroundStatus } returns
            ConnectivityManagerCompat.RESTRICT_BACKGROUND_STATUS_DISABLED
        downloadLanguagesFeature.start()
        downloadLanguagesFeature.connectivityManager = connectivityManager

        downloadLanguagesFeature.wifiConnectedListener(true)

        Mockito.verify(dataSaverAndWifiChanged).invoke(false)
    }
}
