/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.qr

import android.Manifest.permission.CAMERA
import android.content.pm.PackageManager
import androidx.fragment.app.FragmentManager
import androidx.fragment.app.FragmentTransaction
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.feature.qr.QrFeature.Companion.QR_FRAGMENT_TAG
import mozilla.components.support.base.feature.OnNeedToRequestPermissions
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.grantPermission
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mock
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions
import org.mockito.MockitoAnnotations.initMocks

@RunWith(AndroidJUnit4::class)
class QrFeatureTest {

    @Mock
    lateinit var fragmentManager: FragmentManager

    @Before
    fun setUp() {
        initMocks(this)

        mock<FragmentTransaction>().let { transaction ->
            whenever(fragmentManager.beginTransaction())
                .thenReturn(transaction)
            whenever(transaction.add(anyInt(), any(), anyString()))
                .thenReturn(transaction)
            whenever(transaction.remove(any()))
                .thenReturn(transaction)
        }
    }

    @Test
    fun `feature requests camera permission if required`() {
        // Given
        val permissionsCallback = mock<OnNeedToRequestPermissions>()
        val feature = QrFeature(
            testContext,
            fragmentManager,
            onNeedToRequestPermissions = permissionsCallback
        )

        // When
        val scanResult = feature.scan()

        // Then
        assertFalse(scanResult)
        verify(permissionsCallback).invoke(arrayOf(CAMERA))
    }

    @Test
    fun `scan starts qr fragment if permissions granted`() {
        // Given
        grantPermission(CAMERA)
        val feature = QrFeature(
            testContext,
            fragmentManager
        )

        // When
        val scanResult = feature.scan()

        // Then
        assertTrue(scanResult)
        verify(fragmentManager).beginTransaction()
    }

    @Test
    fun `onPermissionsResult displays scanner only if permission granted`() {
        // Given
        val feature = QrFeature(
            testContext,
            fragmentManager
        )

        // When
        resolvePermissionRequestFrom(feature) { PermissionResolution.DENIED }

        // Then
        verify(fragmentManager, never()).beginTransaction()

        // When
        grantPermission(CAMERA)
        resolvePermissionRequestFrom(feature) { PermissionResolution.GRANTED }

        // Then
        verify(fragmentManager).beginTransaction()
    }

    @Test
    fun `scan result is forwarded to caller`() {
        // Given
        val scanResultCallback = spy(mock<OnScanResult>())
        val feature = QrFeature(
            testContext,
            fragmentManager,
            onScanResult = scanResultCallback
        )

        // When
        feature.scanCompleteListener.onScanComplete("result")

        // Then
        verify(scanResultCallback).invoke("result")
    }

    @Test
    fun `qr fragment is removed on back pressed`() {
        // Given
        whenever(fragmentManager.findFragmentByTag(QR_FRAGMENT_TAG))
            .thenReturn(mock())

        val feature = spy(
            QrFeature(
                testContext,
                fragmentManager
            )
        )

        // When
        feature.onBackPressed()

        // Then
        verify(feature).removeQrFragment()
    }

    @Test
    fun `start attaches scan complete listener`() {
        // Given
        val fragment = mock<QrFragment>()
        whenever(fragmentManager.findFragmentByTag(QR_FRAGMENT_TAG))
            .thenReturn(fragment)

        val feature = QrFeature(
            testContext,
            fragmentManager
        )

        // When
        feature.start()

        // Then
        verify(fragment).scanCompleteListener = feature.scanCompleteListener
    }

    @Test
    fun `do nothing when stop() is called`() {
        // Given
        val scanResultCallback = mock<OnScanResult>()
        val feature = QrFeature(
            testContext,
            fragmentManager,
            onScanResult = scanResultCallback
        )

        // When
        feature.stop()

        // Then
        verifyNoMoreInteractions(scanResultCallback, fragmentManager)
    }
}

private enum class PermissionResolution(val value: Int) {
    GRANTED(PackageManager.PERMISSION_GRANTED),
    DENIED(PackageManager.PERMISSION_DENIED)
}

private fun resolvePermissionRequestFrom(
    feature: QrFeature,
    resolution: () -> PermissionResolution
) {
    feature.onPermissionsResult(emptyArray(), IntArray(1) { resolution().value })
}
