/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.digitalassetlinks

import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import android.content.pm.Signature
import android.content.pm.SigningInfo
import android.os.Build
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mock
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.MockitoAnnotations
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
class AndroidAssetFinderTest {

    private lateinit var assetFinder: AndroidAssetFinder
    private lateinit var packageInfo: PackageInfo
    @Mock lateinit var packageManager: PackageManager
    @Mock lateinit var signingInfo: SigningInfo

    @Before
    fun setup() {
        assetFinder = spy(AndroidAssetFinder())

        MockitoAnnotations.openMocks(this)
        packageInfo = PackageInfo()
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
    }

    @Test
    fun `test getAndroidAppAsset returns empty list if name not found`() {
        `when`(packageManager.getPackageInfo(anyString(), anyInt()))
            .thenThrow(PackageManager.NameNotFoundException::class.java)

        assertEquals(
            emptyList<AssetDescriptor.Android>(),
            assetFinder.getAndroidAppAsset("com.test.app", packageManager).toList()
        )
    }

    @Config(sdk = [Build.VERSION_CODES.P])
    @Test
    fun `test getAndroidAppAsset on P SDK`() {
        val signature = mock<Signature>()
        packageInfo.signingInfo = signingInfo
        `when`(signingInfo.hasMultipleSigners()).thenReturn(false)
        `when`(signingInfo.signingCertificateHistory).thenReturn(arrayOf(signature, mock()))
        doReturn("01:BB:AA:10:30").`when`(assetFinder).getCertificateSHA256Fingerprint(signature)

        assertEquals(
            listOf(AssetDescriptor.Android("com.test.app", "01:BB:AA:10:30")),
            assetFinder.getAndroidAppAsset("com.test.app", packageManager).toList()
        )
    }

    @Config(sdk = [Build.VERSION_CODES.P])
    @Test
    fun `test getAndroidAppAsset with multiple signatures on P SDK`() {
        val signature1 = mock<Signature>()
        val signature2 = mock<Signature>()
        packageInfo.signingInfo = signingInfo
        `when`(signingInfo.hasMultipleSigners()).thenReturn(true)
        `when`(signingInfo.apkContentsSigners).thenReturn(arrayOf(signature1, signature2))
        doReturn("01:BB:AA:10:30").`when`(assetFinder).getCertificateSHA256Fingerprint(signature1)
        doReturn("FF:CC:AA:99:77").`when`(assetFinder).getCertificateSHA256Fingerprint(signature2)

        assertEquals(
            listOf(
                AssetDescriptor.Android("org.test.app", "01:BB:AA:10:30"),
                AssetDescriptor.Android("org.test.app", "FF:CC:AA:99:77")
            ),
            assetFinder.getAndroidAppAsset("org.test.app", packageManager).toList()
        )
    }

    @Config(sdk = [Build.VERSION_CODES.P])
    @Test
    fun `test getAndroidAppAsset with empty history`() {
        packageInfo.signingInfo = signingInfo
        `when`(signingInfo.hasMultipleSigners()).thenReturn(false)
        `when`(signingInfo.signingCertificateHistory).thenReturn(emptyArray())

        assertEquals(
            emptyList<AssetDescriptor.Android>(),
            assetFinder.getAndroidAppAsset("com.test.app", packageManager).toList()
        )
    }

    @Config(sdk = [Build.VERSION_CODES.LOLLIPOP])
    @Suppress("Deprecation")
    @Test
    fun `test getAndroidAppAsset on deprecated SDK`() {
        val signature = mock<Signature>()
        packageInfo.signatures = arrayOf(signature)
        doReturn("01:BB:AA:10:30").`when`(assetFinder).getCertificateSHA256Fingerprint(signature)

        assertEquals(
            listOf(AssetDescriptor.Android("com.test.app", "01:BB:AA:10:30")),
            assetFinder.getAndroidAppAsset("com.test.app", packageManager).toList()
        )
    }

    @Config(sdk = [Build.VERSION_CODES.LOLLIPOP])
    @Suppress("Deprecation")
    @Test
    fun `test getAndroidAppAsset with multiple signatures on deprecated SDK`() {
        val signature1 = mock<Signature>()
        val signature2 = mock<Signature>()
        packageInfo.signatures = arrayOf(signature1, signature2)
        doReturn("01:BB:AA:10:30").`when`(assetFinder).getCertificateSHA256Fingerprint(signature1)
        doReturn("FF:CC:AA:99:77").`when`(assetFinder).getCertificateSHA256Fingerprint(signature2)

        assertEquals(
            listOf(
                AssetDescriptor.Android("org.test.app", "01:BB:AA:10:30"),
                AssetDescriptor.Android("org.test.app", "FF:CC:AA:99:77")
            ),
            assetFinder.getAndroidAppAsset("org.test.app", packageManager).toList()
        )
    }

    @Config(sdk = [Build.VERSION_CODES.LOLLIPOP])
    @Suppress("Deprecation")
    @Test
    fun `test getAndroidAppAsset is lazily computed`() {
        val signature1 = mock<Signature>()
        val signature2 = mock<Signature>()
        packageInfo.signatures = arrayOf(signature1, signature2)
        doReturn("01:BB:AA:10:30").`when`(assetFinder).getCertificateSHA256Fingerprint(signature1)
        doReturn("FF:CC:AA:99:77").`when`(assetFinder).getCertificateSHA256Fingerprint(signature2)

        val result = assetFinder.getAndroidAppAsset("android.package", packageManager).first()
        assertEquals(
            AssetDescriptor.Android("android.package", "01:BB:AA:10:30"),
            result
        )

        verify(assetFinder, times(1)).getCertificateSHA256Fingerprint(any())
    }

    @Test
    fun `test byteArrayToHexString`() {
        val array = byteArrayOf(0xaa.toByte(), 0xbb.toByte(), 0xcc.toByte(), 0x10, 0x20, 0x30, 0x01, 0x02)
        assertEquals(
            "AA:BB:CC:10:20:30:01:02",
            assetFinder.byteArrayToHexString(array)
        )
    }
}
