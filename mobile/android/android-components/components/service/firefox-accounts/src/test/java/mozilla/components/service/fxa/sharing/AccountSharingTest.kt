/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.sharing

import android.content.ContentProviderClient
import android.content.ContentResolver
import android.content.Context
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import android.content.pm.Signature
import android.content.pm.SigningInfo
import android.database.Cursor
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.sync.MigratingAccountInfo
import mozilla.components.service.fxa.sharing.AccountSharing.KEY_EMAIL
import mozilla.components.service.fxa.sharing.AccountSharing.KEY_KSYNC
import mozilla.components.service.fxa.sharing.AccountSharing.KEY_KXSCS
import mozilla.components.service.fxa.sharing.AccountSharing.KEY_SESSION_TOKEN
import mozilla.components.support.ktx.kotlin.toHexString
import mozilla.components.support.ktx.kotlin.toSha256Digest
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.`when`

@RunWith(AndroidJUnit4::class)
class AccountSharingTest {

    @Test
    fun `getSignaturePostAPI28 - return null if app does not exist`() {
        val packageManager: PackageManager = mock()
        val packageName = "org.mozilla.firefox"

        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenThrow(PackageManager.NameNotFoundException())
        assertNull(AccountSharing.getSignaturePostAPI28(packageManager, packageName))
    }

    @Test
    fun `getSignaturePostAPI28 - return null if it has multiple signers`() {
        val packageManager: PackageManager = mock()
        val packageName = "org.mozilla.firefox"
        val packageInfo: PackageInfo = mock()
        val signingInfo: SigningInfo = mock()
        packageInfo.signingInfo = signingInfo

        `when`(signingInfo.hasMultipleSigners()).thenReturn(true)
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        assertNull(AccountSharing.getSignaturePostAPI28(packageManager, packageName))
    }

    @Test
    fun `getSignaturePostAPI28 - return null if certificate rotation was performed`() {
        val packageManager: PackageManager = mock()
        val packageName = "org.mozilla.firefox"
        val packageInfo: PackageInfo = mock()
        val signingInfo: SigningInfo = mock()
        packageInfo.signingInfo = signingInfo

        `when`(signingInfo.hasMultipleSigners()).thenReturn(false)
        `when`(signingInfo.hasPastSigningCertificates()).thenReturn(true)
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        assertNull(AccountSharing.getSignaturePostAPI28(packageManager, packageName))
    }

    @Test
    fun `getSignaturePostAPI28 - return first available signature`() {
        val packageManager: PackageManager = mock()
        val packageName = "org.mozilla.firefox"
        val packageInfo: PackageInfo = mock()
        val signingInfo: SigningInfo = mock()
        val signatureValue = "308201".toByteArray()
        val signature = Signature(signatureValue)
        val signatures = arrayOf(signature)

        packageInfo.signingInfo = signingInfo

        `when`(signingInfo.signingCertificateHistory).thenReturn(signatures)
        `when`(signingInfo.hasMultipleSigners()).thenReturn(false)
        `when`(signingInfo.hasPastSigningCertificates()).thenReturn(false)
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        val result = AccountSharing.getSignaturePostAPI28(packageManager, packageName)

        assertEquals(signatureValue.toSha256Digest().toHexString(), result)
    }

    @Test
    fun `getSignaturePreAPI28 - return null if app does not exist`() {
        val packageManager: PackageManager = mock()
        val packageName = "org.mozilla.firefox"

        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenThrow(PackageManager.NameNotFoundException())
        assertNull(AccountSharing.getSignaturePreAPI28(packageManager, packageName))
    }

    @Test
    @Suppress("DEPRECATION")
    fun `getSignaturePreAPI28 - return null if it has multiple signers`() {
        val packageManager: PackageManager = mock()
        val packageName = "org.mozilla.firefox"
        val packageInfo: PackageInfo = mock()
        packageInfo.signatures = arrayOf(Signature("00"), Signature("11"))

        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        assertNull(AccountSharing.getSignaturePreAPI28(packageManager, packageName))
    }

    @Test
    @Suppress("DEPRECATION")
    fun `getSignaturePreAPI28 - return first available signature`() {
        val packageManager: PackageManager = mock()
        val packageName = "org.mozilla.firefox"
        val packageInfo: PackageInfo = mock()
        val signatureValue = "308201".toByteArray()
        packageInfo.signatures = arrayOf(Signature(signatureValue))

        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        val result = AccountSharing.getSignaturePreAPI28(packageManager, packageName)
        assertEquals(signatureValue.toSha256Digest().toHexString(), result)
    }

    @Test
    fun queryMigratableAccounts() {
        val context: Context = mock()
        val packageNameRelease = "org.mozilla.firefox"
        val packageNameBeta = "org.mozilla.firefox_beta"
        val packageManager: PackageManager = mock()
        val packageInfo: PackageInfo = mock()
        val signingInfo: SigningInfo = mock()
        // This is unhashed signature value
        val signatureValue = "308203a63082028ea00302010202044c72fd88300d06092a864886f70d0101050500308194310b3009060355040613025553311330110603550408130a43616c69666f726e6961311630140603550407130d4d6f756e7461696e2056696577311c301a060355040a13134d6f7a696c6c6120436f72706f726174696f6e311c301a060355040b131352656c6561736520456e67696e656572696e67311c301a0603550403131352656c6561736520456e67696e656572696e67301e170d3130303832333233303032345a170d3338303130383233303032345a308194310b3009060355040613025553311330110603550408130a43616c69666f726e6961311630140603550407130d4d6f756e7461696e2056696577311c301a060355040a13134d6f7a696c6c6120436f72706f726174696f6e311c301a060355040b131352656c6561736520456e67696e656572696e67311c301a0603550403131352656c6561736520456e67696e656572696e6730820122300d06092a864886f70d01010105000382010f003082010a0282010100b4160fb324eac03bd9f7ca21a094769d6811d5e9de2a2314b86f279b7de05b086465ec2dda1db2023bc1b33f73e92c52ed185bb95fcb5d2d81667f6e76266e76de836b3e928d94dd9675bb6ec051fc378affae85158e4ffad4ed27c9f3efc8fa7641ff08e43b4c56ded176d981cb83cf87002d0fe55ab00753f8f255b52f04b9d30173fc2c9b980b6ea24d1ae62e0fe0e73e692591e4f4d5701739929d91c6874ccb932bd533ba3f45586a2306bd39e7aa02fa90c0271a50fa3bde8fe4dd820fe8143a18795717349cfc32e9ceecbd01323c7c86f3132b140341bfc6dc26c0559127169510b0181cfa81b5491dd7c9dc0de3f2ab06b8dcdd7331692839f865650203010001300d06092a864886f70d010105050003820101007a06b17b9f5e49cfe86fc7bd9155b9e755d2f770802c3c9d70bde327bb54f5d7e41e8bb1a466dac30e9933f249ba9f06240794d56af9b36afb01e2272f57d14e9ca162733b0dd8ba373fb465428c5cfe14376f08e58d65c82f18f6c26255519f5244c3c34c9f552e1fcb52f71bcc62180f53e8027221af716be5adc55b939478725c12cb688bad617168d0f803513a6c10be147250ed7b5d2d37569135e81ceca38bba7bdcb5f9a802bae6740d85ae0a4c3fb27da78cc5b8c2fae4d8f361894ac70236bdcb3eadf9f36f46ee48662f9be4e22eda49e1b4db1e911ab972d8925298f16e831344da881059a9c0fbce229efeae719740e975d7f0dc691ccca0a9ce"
        val signature = Signature(signatureValue)
        val signatures = arrayOf(signature)
        val contentResolver: ContentResolver = mock()
        val contentProviderClient: ContentProviderClient = mock()
        packageInfo.signingInfo = signingInfo

        `when`(signingInfo.signingCertificateHistory).thenReturn(signatures)
        `when`(signingInfo.hasMultipleSigners()).thenReturn(false)
        `when`(signingInfo.hasPastSigningCertificates()).thenReturn(false)
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)
        `when`(contentResolver.acquireContentProviderClient("$packageNameRelease.fxa.auth"))
            .thenReturn(contentProviderClient)
        `when`(contentResolver.acquireContentProviderClient("$packageNameBeta.fxa.auth"))
            .thenReturn(contentProviderClient)
        `when`(context.contentResolver).thenReturn(contentResolver)

        // Account without tokens should not be returned
        val expiredAccount = createAccount("user@mozilla.org")
        `when`(contentProviderClient.query(any(), any(), any(), any(), any())).thenReturn(expiredAccount)
        var result = AccountSharing.queryShareableAccounts(context)
        assertTrue(result.isEmpty())

        // Accounts with valid tokens should be returned in order
        val validAccount = createAccount("user@mozilla.org", "sessionToken", "ksync", "kxscs")
        `when`(contentProviderClient.query(any(), any(), any(), any(), any())).thenReturn(validAccount)
        result = AccountSharing.queryShareableAccounts(context)
        assertEquals(2, result.size)
        val expectedAccountRelease = ShareableAccount(
            "user@mozilla.org",
            packageNameRelease,
            MigratingAccountInfo(
                "sessionToken".toByteArray().toHexString(),
                "ksync".toByteArray().toHexString(),
                "kxscs",
            ),
        )
        assertEquals(expectedAccountRelease, result[0])

        val expectedAccountBeta = ShareableAccount(
            "user@mozilla.org",
            packageNameBeta,
            MigratingAccountInfo(
                "sessionToken".toByteArray().toHexString(),
                "ksync".toByteArray().toHexString(),
                "kxscs",
            ),
        )
        assertEquals(expectedAccountBeta, result[1])
    }

    private fun createAccount(
        email: String? = null,
        sessionToken: String? = null,
        ksync: String? = null,
        kxscs: String? = null,
    ): Cursor {
        val cursor: Cursor = mock()
        `when`(cursor.getColumnIndex(KEY_EMAIL)).thenReturn(0)
        `when`(cursor.getColumnIndex(KEY_SESSION_TOKEN)).thenReturn(1)
        `when`(cursor.getColumnIndex(KEY_KSYNC)).thenReturn(2)
        `when`(cursor.getColumnIndex(KEY_KXSCS)).thenReturn(3)
        `when`(cursor.getString(0)).thenReturn(email)
        `when`(cursor.getBlob(1)).thenReturn(sessionToken?.toByteArray())
        `when`(cursor.getBlob(2)).thenReturn(ksync?.toByteArray())
        `when`(cursor.getString(3)).thenReturn(kxscs)
        return cursor
    }
}
