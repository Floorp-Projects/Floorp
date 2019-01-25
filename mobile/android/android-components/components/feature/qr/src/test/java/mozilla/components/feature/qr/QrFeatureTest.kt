/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.qr

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.support.v4.app.FragmentManager
import android.support.v4.app.FragmentTransaction
import androidx.test.core.app.ApplicationProvider
import mozilla.components.feature.qr.QrFeature.Companion.QR_FRAGMENT_TAG
import mozilla.components.support.test.any
import org.junit.Assert.assertTrue
import mozilla.components.support.test.robolectric.grantPermission
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class QrFeatureTest {

    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    @Test
    fun `feature requests permission if required`() {
        val fragmentManager = mockFragmentManager()

        var permissionRequested = false

        val feature = QrFeature(context,
            fragmentManager = fragmentManager,
            onNeedToRequestPermissions = { permissionRequested = true }
        )

        assertFalse(feature.scan())
        assertTrue(permissionRequested)
    }

    @Test
    fun `scan starts qr fragment if permissions granted`() {
        val fragmentManager = mockFragmentManager()
        grantPermission(Manifest.permission.CAMERA)

        val feature = QrFeature(context,
            fragmentManager = fragmentManager,
            onNeedToRequestPermissions = { },
            onScanResult = { }
        )

        assertTrue(feature.scan(123))
        verify(fragmentManager).beginTransaction()
    }

    @Test
    fun `onPermissionsResult displays scanner only if permission granted`() {
        val fragmentManager = mockFragmentManager()

        val feature = QrFeature(context, fragmentManager = fragmentManager)

        assertFalse(feature.scan())

        feature.onPermissionsResult(emptyArray(), IntArray(1) { PackageManager.PERMISSION_DENIED })
        verify(fragmentManager, never()).beginTransaction()

        grantPermission(Manifest.permission.CAMERA)
        feature.onPermissionsResult(emptyArray(), IntArray(1) { PackageManager.PERMISSION_GRANTED })
        verify(fragmentManager).beginTransaction()
    }

    @Test
    fun `scan result is forwarded to caller`() {
        var scanResultReceived = ""

        val feature = QrFeature(context,
            fragmentManager = mockFragmentManager(),
            onScanResult = { result -> scanResultReceived = result }
        )

        feature.scan()
        feature.scanCompleteListener.onScanComplete("result")

        assertEquals("result", scanResultReceived)
    }

    @Test
    fun `qr fragment is removed on back pressed`() {
        val fragmentManager = mockFragmentManager()
        val fragment: QrFragment = mock()
        `when`(fragmentManager.findFragmentByTag(QR_FRAGMENT_TAG)).thenReturn(fragment)

        val feature = spy(
            QrFeature(
                context,
                fragmentManager = fragmentManager,
                onScanResult = { }
            )
        )

        feature.onBackPressed()
        verify(feature).removeQrFragment()
    }

    @Test
    fun `start attaches scan complete listener`() {
        val fragmentManager = mockFragmentManager()

        val feature = QrFeature(context, fragmentManager = fragmentManager, onScanResult = { })
        feature.start()

        val fragment: QrFragment = mock()
        `when`(fragmentManager.findFragmentByTag(QR_FRAGMENT_TAG)).thenReturn(fragment)
        feature.start()

        verify(fragment).scanCompleteListener = feature.scanCompleteListener
    }

    private fun mockFragmentManager(): FragmentManager {
        val fragmentManager: FragmentManager = mock()

        val transaction: FragmentTransaction = mock()
        doReturn(transaction).`when`(fragmentManager).beginTransaction()
        doReturn(transaction).`when`(transaction).add(anyInt(), any(), anyString())
        doReturn(transaction).`when`(transaction).remove(any())

        return fragmentManager
    }
}