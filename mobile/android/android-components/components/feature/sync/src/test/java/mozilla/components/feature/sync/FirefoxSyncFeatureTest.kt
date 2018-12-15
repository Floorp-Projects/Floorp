/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sync

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.storage.SyncError
import mozilla.components.concept.storage.SyncOk
import mozilla.components.concept.storage.SyncStatus
import mozilla.components.concept.storage.SyncableStore
import mozilla.components.service.fxa.AccessTokenInfo
import mozilla.components.service.fxa.FirefoxAccountShaped
import mozilla.components.service.fxa.OAuthScopedKey
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Test
import org.junit.Assert.assertTrue
import org.mockito.Mockito.`when`
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import java.lang.Exception

class FirefoxSyncFeatureTest {
    data class TestAuthType(
        val kid: String,
        val fxaAccessToken: String,
        val syncKey: String,
        val tokenServerUrl: String
    )

    private fun FxaAuthInfo.into(): TestAuthType {
        return TestAuthType(this.kid, this.fxaAccessToken, this.syncKey, this.tokenServerUrl)
    }

    @Test
    fun `sync with no stores`() = runBlocking {
        val feature = FirefoxSyncFeature(mapOf()) { it.into() }
        val results = feature.sync(mock())
        assertTrue(results.isEmpty())
    }

    @Test
    fun `sync with configured stores`() = runBlocking {
        val mockAccount: FirefoxAccountShaped = mock()
        `when`(mockAccount.getTokenServerEndpointURL()).thenReturn("dummyUrl")

        val mockAccessTokenInfo = AccessTokenInfo(
            key = OAuthScopedKey("kid", "k"), token = "token", expiresAt = 0
        )
        `when`(mockAccount.getAccessToken(any())).thenReturn(CompletableDeferred((mockAccessTokenInfo)))

        // Single store, different result types.
        val testStore: SyncableStore<TestAuthType> = mock()
        val feature = FirefoxSyncFeature(mapOf("testStore" to testStore)) { it.into() }

        `when`(testStore.sync(any())).thenReturn(SyncOk)
        var results = feature.sync(mockAccount)
        assertTrue(results["testStore"]!!.status is SyncOk)
        assertEquals(1, results.size)

        `when`(testStore.sync(any())).thenReturn(SyncError(Exception("test")))
        results = feature.sync(mockAccount)
        var error = results["testStore"]!!.status
        assertTrue(error is SyncError)
        assertEquals("test", (error as SyncError).exception.message)
        assertEquals(1, results.size)

        // Multiple stores, different result types.
        val anotherStore: SyncableStore<TestAuthType> = mock()
        val anotherFeature = FirefoxSyncFeature(mapOf(
            Pair("testStore", testStore),
            Pair("goodStore", anotherStore))
        ) { it.into() }

        `when`(anotherStore.sync(any())).thenReturn(SyncOk)

        results = anotherFeature.sync(mockAccount)
        assertEquals(2, results.size)
        error = results["testStore"]!!.status
        assertTrue(error is SyncError)
        assertEquals("test", (error as SyncError).exception.message)
        assertTrue(results["goodStore"]!!.status is SyncOk)
    }

    @Test
    fun `sync status can be observed`() = runBlocking {
        val mockAccount: FirefoxAccountShaped = mock()
        `when`(mockAccount.getTokenServerEndpointURL()).thenReturn("dummyUrl")

        val mockAccessTokenInfo = AccessTokenInfo(
                key = OAuthScopedKey("kid", "k"), token = "token", expiresAt = 0
        )
        `when`(mockAccount.getAccessToken(any())).thenReturn(CompletableDeferred((mockAccessTokenInfo)))

        val verifier = object {
            private val blocks = mutableListOf<() -> Unit>()

            fun addVerifyBlock(block: () -> Unit) {
                blocks.add(block)
            }

            fun verify() {
                blocks.forEach { it() }
            }
        }

        // A store that runs verifications during a sync.
        val testStore = object : SyncableStore<TestAuthType> {
            override suspend fun sync(authInfo: TestAuthType): SyncStatus {
                verifier.verify()
                return SyncOk
            }
        }
        val syncStatusObserver: SyncStatusObserver = mock()

        val feature = FirefoxSyncFeature(mapOf("testStore" to testStore)) { it.into() }

        // These assertions will run while sync is in progress.
        verifier.addVerifyBlock {
            assertTrue(feature.syncRunning())
            verify(syncStatusObserver, times(1)).onStarted()
            verify(syncStatusObserver, never()).onIdle()
        }

        feature.register(syncStatusObserver)
        verify(syncStatusObserver, never()).onStarted()
        verify(syncStatusObserver, never()).onIdle()
        assertFalse(feature.syncRunning())

        feature.sync(mockAccount)

        verify(syncStatusObserver, times(1)).onStarted()
        verify(syncStatusObserver, times(1)).onIdle()
        assertFalse(feature.syncRunning())
    }
}
