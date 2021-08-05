/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.manager.MigrationResult
import mozilla.components.service.fxa.sharing.ShareableAccount
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.json.JSONException
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoInteractions
import java.io.File

@RunWith(AndroidJUnit4::class)
class FennecFxaMigrationTest {
    @Test
    fun `no state`() = runBlocking {
        val fxaPath = File(getTestPath("fxa"), "no-file.json")
        val accountManager: FxaAccountManager = mock()

        with(FennecFxaMigration.migrate(fxaPath, testContext, accountManager) as Result.Success) {
            assertTrue(this.value is FxaMigrationResult.Success.NoAccount)
        }

        verifyNoInteractions(accountManager)
    }

    @Test
    fun `separated fxa state v4`() = runBlocking {
        val fxaPath = File(getTestPath("fxa"), "separated-v4.json")
        val accountManager: FxaAccountManager = mock()

        with(FennecFxaMigration.migrate(fxaPath, testContext, accountManager) as Result.Success) {
            assertEquals(FxaMigrationResult.Success.UnauthenticatedAccount::class, this.value::class)

            val state = this.value as FxaMigrationResult.Success.UnauthenticatedAccount

            assertEquals("test@example.com", state.email)
            assertEquals("Separated", state.stateLabel)
            assertEquals("Unauthenticated account in state Separated", state.toString())
        }

        verifyNoInteractions(accountManager)
    }

    @Test
    fun `doghouse fxa state v4`() = runBlocking {
        val fxaPath = File(getTestPath("fxa"), "doghouse-v4.json")
        val accountManager: FxaAccountManager = mock()

        with(FennecFxaMigration.migrate(fxaPath, testContext, accountManager) as Result.Success) {
            assertEquals(FxaMigrationResult.Success.UnauthenticatedAccount::class, this.value::class)

            val state = this.value as FxaMigrationResult.Success.UnauthenticatedAccount

            assertEquals("test@example.com", state.email)
            assertEquals("Doghouse", state.stateLabel)
            assertEquals("Unauthenticated account in state Doghouse", state.toString())
        }

        verifyNoInteractions(accountManager)
    }

    @Test
    fun `married fxa state v4 successfull sign-in`() = runBlocking {
        val fxaPath = File(getTestPath("fxa"), "married-v4.json")
        val accountManager: FxaAccountManager = mock()

        `when`(accountManager.migrateFromAccount(any(), eq(true))).thenReturn(MigrationResult.Success)

        with(FennecFxaMigration.migrate(fxaPath, testContext, accountManager) as Result.Success) {
            assertEquals(FxaMigrationResult.Success.SignedInIntoAuthenticatedAccount::class, this.value::class)
            assertEquals("test@example.com", (this.value as FxaMigrationResult.Success.SignedInIntoAuthenticatedAccount).email)
            assertEquals("Married", (this.value as FxaMigrationResult.Success.SignedInIntoAuthenticatedAccount).stateLabel)

            val captor = argumentCaptor<ShareableAccount>()
            verify(accountManager).migrateFromAccount(captor.capture(), eq(true))

            assertEquals("test@example.com", captor.value.email)
            assertEquals("252fsvj8932vj32movj97325hjfksdhfjstrg23yurt267r23", captor.value.authInfo.kSync)
            assertEquals("0b3ba79bfxdf32f3of32jowef7987f", captor.value.authInfo.kXCS)
            assertEquals("fjsdkfksf3e8f32f23f832fwf32jf89o327u2843gj23", captor.value.authInfo.sessionToken)
        }
    }

    @Test
    fun `cohabiting fxa state v4 successful sign-in`() = runBlocking {
        val fxaPath = File(getTestPath("fxa"), "cohabiting-v4.json")
        val accountManager: FxaAccountManager = mock()

        `when`(accountManager.migrateFromAccount(any(), eq(true))).thenReturn(MigrationResult.Success)

        with(FennecFxaMigration.migrate(fxaPath, testContext, accountManager) as Result.Success) {
            assertEquals(FxaMigrationResult.Success.SignedInIntoAuthenticatedAccount::class, this.value::class)
            assertEquals("test@example.com", (this.value as FxaMigrationResult.Success.SignedInIntoAuthenticatedAccount).email)
            assertEquals("Cohabiting", (this.value as FxaMigrationResult.Success.SignedInIntoAuthenticatedAccount).stateLabel)

            val captor = argumentCaptor<ShareableAccount>()
            verify(accountManager).migrateFromAccount(captor.capture(), eq(true))

            assertEquals("test@example.com", captor.value.email)
            assertEquals("252bc4ccc3a239fsdfsdf32fg32wf3w4e3472d41d1a204890", captor.value.authInfo.kSync)
            assertEquals("0b3ba79b18bd9fsdfsdf4g234adedd87", captor.value.authInfo.kXCS)
            assertEquals("fsdfjsdffsdf342f23g3ogou97328uo23ij", captor.value.authInfo.sessionToken)
        }
    }

    @Test
    fun `cohabiting fxa state v4 will retry sign-in`() = runBlocking {
        val fxaPath = File(getTestPath("fxa"), "cohabiting-v4.json")
        val accountManager: FxaAccountManager = mock()

        `when`(accountManager.migrateFromAccount(any(), eq(true))).thenReturn(MigrationResult.WillRetry)

        with(FennecFxaMigration.migrate(fxaPath, testContext, accountManager) as Result.Success) {
            assertEquals(FxaMigrationResult.Success.WillAutoRetrySignInLater::class, this.value::class)
            assertEquals("test@example.com", (this.value as FxaMigrationResult.Success.WillAutoRetrySignInLater).email)
            assertEquals("Cohabiting", (this.value as FxaMigrationResult.Success.WillAutoRetrySignInLater).stateLabel)

            val captor = argumentCaptor<ShareableAccount>()
            verify(accountManager).migrateFromAccount(captor.capture(), eq(true))

            assertEquals("test@example.com", captor.value.email)
            assertEquals("252bc4ccc3a239fsdfsdf32fg32wf3w4e3472d41d1a204890", captor.value.authInfo.kSync)
            assertEquals("0b3ba79b18bd9fsdfsdf4g234adedd87", captor.value.authInfo.kXCS)
            assertEquals("fsdfjsdffsdf342f23g3ogou97328uo23ij", captor.value.authInfo.sessionToken)
        }
    }

    @Test
    fun `married fxa state v4 failed sign-in`() = runBlocking {
        val fxaPath = File(getTestPath("fxa"), "married-v4.json")
        val accountManager: FxaAccountManager = mock()

        `when`(accountManager.migrateFromAccount(any(), eq(true))).thenReturn(MigrationResult.Failure)

        with(FennecFxaMigration.migrate(fxaPath, testContext, accountManager) as Result.Failure) {
            val unwrapped = this.throwables.first() as FxaMigrationException
            assertEquals(FxaMigrationResult.Failure.FailedToSignIntoAuthenticatedAccount::class, unwrapped.failure::class)
            val unwrappedFailure = unwrapped.failure as FxaMigrationResult.Failure.FailedToSignIntoAuthenticatedAccount
            assertEquals("test@example.com", unwrappedFailure.email)
            assertEquals("Married", (unwrapped.failure as FxaMigrationResult.Failure.FailedToSignIntoAuthenticatedAccount).stateLabel)

            val captor = argumentCaptor<ShareableAccount>()
            verify(accountManager).migrateFromAccount(captor.capture(), eq(true))

            assertEquals("test@example.com", captor.value.email)
            assertEquals("252fsvj8932vj32movj97325hjfksdhfjstrg23yurt267r23", captor.value.authInfo.kSync)
            assertEquals("0b3ba79bfxdf32f3of32jowef7987f", captor.value.authInfo.kXCS)
            assertEquals("fjsdkfksf3e8f32f23f832fwf32jf89o327u2843gj23", captor.value.authInfo.sessionToken)
        }
    }

    @Test
    fun `custom token server`() = runBlocking {
        val fxaPath = File(getTestPath("fxa"), "custom-sync-config-token.json")
        val accountManager: FxaAccountManager = mock()

        with(FennecFxaMigration.migrate(fxaPath, testContext, accountManager) as Result.Failure) {
            val unwrapped = this.throwables.first() as FxaMigrationException
            assertEquals(FxaMigrationResult.Failure.CustomServerConfigPresent::class, unwrapped.failure::class)
            val unwrappedFailure = unwrapped.failure as FxaMigrationResult.Failure.CustomServerConfigPresent
            assertFalse(unwrappedFailure.customIdpServer)
            assertTrue(unwrappedFailure.customTokenServer)
        }
    }

    @Test
    fun `custom idp server`() = runBlocking {
        val fxaPath = File(getTestPath("fxa"), "custom-sync-config-idp.json")
        val accountManager: FxaAccountManager = mock()

        with(FennecFxaMigration.migrate(fxaPath, testContext, accountManager) as Result.Failure) {
            val unwrapped = this.throwables.first() as FxaMigrationException
            assertEquals(FxaMigrationResult.Failure.CustomServerConfigPresent::class, unwrapped.failure::class)
            val unwrappedFailure = unwrapped.failure as FxaMigrationResult.Failure.CustomServerConfigPresent
            assertTrue(unwrappedFailure.customIdpServer)
            assertFalse(unwrappedFailure.customTokenServer)
        }
    }

    @Test
    fun `custom idp and token servers`() = runBlocking {
        val fxaPath = File(getTestPath("fxa"), "custom-sync-config-idp-token.json")
        val accountManager: FxaAccountManager = mock()

        with(FennecFxaMigration.migrate(fxaPath, testContext, accountManager) as Result.Failure) {
            val unwrapped = this.throwables.first() as FxaMigrationException
            assertEquals(FxaMigrationResult.Failure.CustomServerConfigPresent::class, unwrapped.failure::class)
            val unwrappedFailure = unwrapped.failure as FxaMigrationResult.Failure.CustomServerConfigPresent
            assertTrue(unwrappedFailure.customIdpServer)
            assertTrue(unwrappedFailure.customTokenServer)
        }
    }

    @Test
    fun `china idp and token servers`() = runBlocking {
        customServerAssertAllowed("china-sync-config-idp-token.json")
    }

    private suspend fun customServerAssertAllowed(testFile: String) {
        val fxaPath = File(getTestPath("fxa"), testFile)
        val accountManager: FxaAccountManager = mock()

        `when`(accountManager.migrateFromAccount(any(), eq(true))).thenReturn(MigrationResult.Success)

        with(FennecFxaMigration.migrate(fxaPath, testContext, accountManager, true) as Result.Success) {
            assertEquals(FxaMigrationResult.Success.SignedInIntoAuthenticatedAccount::class, this.value::class)
            assertEquals("test@example.com", (this.value as FxaMigrationResult.Success.SignedInIntoAuthenticatedAccount).email)
            assertEquals("Married", (this.value as FxaMigrationResult.Success.SignedInIntoAuthenticatedAccount).stateLabel)

            val captor = argumentCaptor<ShareableAccount>()
            verify(accountManager).migrateFromAccount(captor.capture(), eq(true))

            assertEquals("test@example.com", captor.value.email)
            assertEquals("252fsvj8932vj32movj97325hjfksdhfjstrg23yurt267r23", captor.value.authInfo.kSync)
            assertEquals("0b3ba79bfxdf32f3of32jowef7987f", captor.value.authInfo.kXCS)
            assertEquals("fjsdkfksf3e8f32f23f832fwf32jf89o327u2843gj23", captor.value.authInfo.sessionToken)
        }
    }

    @Test
    fun `custom idp and token servers - allowed, but failed sign-in`() = runBlocking {
        val fxaPath = File(getTestPath("fxa"), "china-sync-config-idp-token.json")
        val accountManager: FxaAccountManager = mock()

        `when`(accountManager.migrateFromAccount(any(), eq(true))).thenReturn(MigrationResult.Failure)

        with(FennecFxaMigration.migrate(fxaPath, testContext, accountManager, true) as Result.Failure) {
            val unwrapped = this.throwables.first() as FxaMigrationException
            assertEquals(FxaMigrationResult.Failure.FailedToSignIntoAuthenticatedAccount::class, unwrapped.failure::class)
            val unwrappedFailure = unwrapped.failure as FxaMigrationResult.Failure.FailedToSignIntoAuthenticatedAccount
            assertEquals("test@example.com", unwrappedFailure.email)
            assertEquals("Married", unwrappedFailure.stateLabel)

            val captor = argumentCaptor<ShareableAccount>()
            verify(accountManager).migrateFromAccount(captor.capture(), eq(true))

            assertEquals("test@example.com", captor.value.email)
            assertEquals("252fsvj8932vj32movj97325hjfksdhfjstrg23yurt267r23", captor.value.authInfo.kSync)
            assertEquals("0b3ba79bfxdf32f3of32jowef7987f", captor.value.authInfo.kXCS)
            assertEquals("fjsdkfksf3e8f32f23f832fwf32jf89o327u2843gj23", captor.value.authInfo.sessionToken)
        }
    }

    @Test
    fun `cohabiting fxa state v4 failed sign-in`() = runBlocking {
        val fxaPath = File(getTestPath("fxa"), "cohabiting-v4.json")
        val accountManager: FxaAccountManager = mock()

        `when`(accountManager.migrateFromAccount(any(), eq(true))).thenReturn(MigrationResult.Failure)

        with(FennecFxaMigration.migrate(fxaPath, testContext, accountManager) as Result.Failure) {
            val unwrapped = this.throwables.first() as FxaMigrationException
            assertEquals(FxaMigrationResult.Failure.FailedToSignIntoAuthenticatedAccount::class, unwrapped.failure::class)
            val unwrappedFailure = unwrapped.failure as FxaMigrationResult.Failure.FailedToSignIntoAuthenticatedAccount
            assertEquals("test@example.com", unwrappedFailure.email)
            assertEquals("Cohabiting", unwrappedFailure.stateLabel)

            val captor = argumentCaptor<ShareableAccount>()
            verify(accountManager).migrateFromAccount(captor.capture(), eq(true))

            assertEquals("test@example.com", captor.value.email)
            assertEquals("252bc4ccc3a239fsdfsdf32fg32wf3w4e3472d41d1a204890", captor.value.authInfo.kSync)
            assertEquals("0b3ba79b18bd9fsdfsdf4g234adedd87", captor.value.authInfo.kXCS)
            assertEquals("fsdfjsdffsdf342f23g3ogou97328uo23ij", captor.value.authInfo.sessionToken)
        }
    }

    @Test
    fun `corrupt married fxa state v4`() = runBlocking {
        val fxaPath = File(getTestPath("fxa"), "corrupt-married-v4.json")
        val accountManager: FxaAccountManager = mock()

        with(FennecFxaMigration.migrate(fxaPath, testContext, accountManager) as Result.Failure) {
            assertEquals(1, this.throwables.size)
            val fxaMigrationException = this.throwables.first()
            assertEquals(FxaMigrationException::class, fxaMigrationException::class)
            assertEquals(
                FxaMigrationResult.Failure.CorruptAccountState::class,
                (fxaMigrationException as FxaMigrationException).failure::class
            )
            val corruptAccountResult = fxaMigrationException.failure as FxaMigrationResult.Failure.CorruptAccountState
            assertEquals(JSONException::class, corruptAccountResult.e::class)

            assertTrue(corruptAccountResult.toString().startsWith("Corrupt account state, exception: org.json.JSONException"))
        }

        verifyNoInteractions(accountManager)
    }

    @Test
    fun `corrupt missing versions fxa state v4`() = runBlocking {
        val fxaPath = File(getTestPath("fxa"), "corrupt-separated-missing-versions-v4.json")
        val accountManager: FxaAccountManager = mock()

        with(FennecFxaMigration.migrate(fxaPath, testContext, accountManager) as Result.Failure) {
            assertEquals(1, this.throwables.size)
            val fxaMigrationException = this.throwables.first()
            assertEquals(FxaMigrationException::class, fxaMigrationException::class)
            assertEquals(
                FxaMigrationResult.Failure.CorruptAccountState::class,
                (fxaMigrationException as FxaMigrationException).failure::class
            )
            val corruptAccountResult = fxaMigrationException.failure as FxaMigrationResult.Failure.CorruptAccountState
            assertEquals(JSONException::class, corruptAccountResult.e::class)

            assertEquals("Corrupt account state, exception: org.json.JSONException: No value for pickle_version", corruptAccountResult.toString())
        }

        verifyNoInteractions(accountManager)
    }

    @Test
    fun `corrupt bad fxa state`() = runBlocking {
        val fxaPath = File(getTestPath("fxa"), "separated-bad-state.json")
        val accountManager: FxaAccountManager = mock()

        with(FennecFxaMigration.migrate(fxaPath, testContext, accountManager) as Result.Failure) {
            assertEquals(1, this.throwables.size)
            val fxaMigrationException = this.throwables.first()
            assertEquals(FxaMigrationException::class, fxaMigrationException::class)
            assertEquals(
                FxaMigrationResult.Failure.CorruptAccountState::class,
                (fxaMigrationException as FxaMigrationException).failure::class
            )
            val corruptAccountResult = fxaMigrationException.failure as FxaMigrationResult.Failure.CorruptAccountState
            assertEquals(JSONException::class, corruptAccountResult.e::class)

            assertEquals("Corrupt account state, exception: org.json.JSONException: No value for version", corruptAccountResult.toString())
        }

        verifyNoInteractions(accountManager)
    }

    @Test
    fun `separated - bad account version`() = runBlocking {
        val fxaPath = File(getTestPath("fxa"), "separated-bad-account-version-v4.json")
        val accountManager: FxaAccountManager = mock()

        with(FennecFxaMigration.migrate(fxaPath, testContext, accountManager) as Result.Failure) {
            assertEquals(1, this.throwables.size)
            val fxaMigrationException = this.throwables.first()
            assertEquals(FxaMigrationException::class, fxaMigrationException::class)
            assertEquals(
                FxaMigrationResult.Failure.UnsupportedVersions::class,
                (fxaMigrationException as FxaMigrationException).failure::class
            )
            val unsupportedVersionsResult = fxaMigrationException.failure as FxaMigrationResult.Failure.UnsupportedVersions
            assertEquals(23, unsupportedVersionsResult.accountVersion)
            assertEquals(3, unsupportedVersionsResult.pickleVersion)
            // We didn't process state after detecting bad account version.
            assertNull(unsupportedVersionsResult.stateVersion)

            assertEquals("Unsupported version(s): account=23, pickle=3, state=null", unsupportedVersionsResult.toString())
        }

        verifyNoInteractions(accountManager)
    }

    @Test
    fun `separated - bad pickle version`() = runBlocking {
        val fxaPath = File(getTestPath("fxa"), "separated-bad-pickle-version-v4.json")
        val accountManager: FxaAccountManager = mock()

        with(FennecFxaMigration.migrate(fxaPath, testContext, accountManager) as Result.Failure) {
            assertEquals(1, this.throwables.size)
            val fxaMigrationException = this.throwables.first()
            assertEquals(FxaMigrationException::class, fxaMigrationException::class)
            assertEquals(
                FxaMigrationResult.Failure.UnsupportedVersions::class,
                (fxaMigrationException as FxaMigrationException).failure::class
            )
            val unsupportedVersionsResult = fxaMigrationException.failure as FxaMigrationResult.Failure.UnsupportedVersions
            assertEquals(3, unsupportedVersionsResult.accountVersion)
            assertEquals(34, unsupportedVersionsResult.pickleVersion)
            // We didn't process state after detecting bad account version.
            assertNull(unsupportedVersionsResult.stateVersion)

            assertEquals("Unsupported version(s): account=3, pickle=34, state=null", unsupportedVersionsResult.toString())
        }

        verifyNoInteractions(accountManager)
    }

    @Test
    fun `separated - bad state version`() = runBlocking {
        val fxaPath = File(getTestPath("fxa"), "separated-bad-state-version-v10.json")
        val accountManager: FxaAccountManager = mock()

        with(FennecFxaMigration.migrate(fxaPath, testContext, accountManager) as Result.Failure) {
            assertEquals(1, this.throwables.size)
            val fxaMigrationException = this.throwables.first()
            assertEquals(FxaMigrationException::class, fxaMigrationException::class)
            assertEquals(
                FxaMigrationResult.Failure.UnsupportedVersions::class,
                (fxaMigrationException as FxaMigrationException).failure::class
            )
            val unsupportedVersionsResult = fxaMigrationException.failure as FxaMigrationResult.Failure.UnsupportedVersions
            assertEquals(3, unsupportedVersionsResult.accountVersion)
            assertEquals(3, unsupportedVersionsResult.pickleVersion)
            assertEquals(10, unsupportedVersionsResult.stateVersion)

            assertEquals("Unsupported version(s): account=3, pickle=3, state=10", unsupportedVersionsResult.toString())
        }

        verifyNoInteractions(accountManager)
    }
}
