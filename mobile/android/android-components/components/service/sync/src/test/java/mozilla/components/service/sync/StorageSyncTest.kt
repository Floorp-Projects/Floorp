/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.sync.AccessTokenInfo
import mozilla.components.concept.sync.AuthInfo
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.OAuthScopedKey
import mozilla.components.concept.sync.SyncError
import mozilla.components.concept.sync.SyncOk
import mozilla.components.concept.sync.SyncStatus
import mozilla.components.concept.sync.SyncStatusObserver
import mozilla.components.concept.sync.SyncableStore
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

class StorageSyncTest {
    private val testAuth = AuthInfo("kid", "token", "key", "server")

    @Test
    fun `sync with no stores`() = runBlocking {
        val feature = StorageSync(mapOf(), "sync-scope") { it }
        val results = feature.sync(mock())
        assertTrue(results.isEmpty())
    }

    @Test
    fun `sync with configured stores`() = runBlocking {
        val mockAccount: OAuthAccount = mock()
        `when`(mockAccount.getTokenServerEndpointURL()).thenReturn("dummyUrl")
        `when`(mockAccount.authInfo("sync-scope")).thenReturn(testAuth)

        val mockAccessTokenInfo = AccessTokenInfo(
            scope = "scope", key = OAuthScopedKey("kty", "scope", "kid", "k"), token = "token", expiresAt = 0
        )
        `when`(mockAccount.getAccessToken(any())).thenReturn(CompletableDeferred((mockAccessTokenInfo)))

        // Single store, different result types.
        val testStore: SyncableStore<AuthInfo> = mock()
        val feature = StorageSync(mapOf("testStore" to testStore), "sync-scope") { it }

        `when`(testStore.sync(any())).thenReturn(SyncOk)
        var results = feature.sync(mockAccount)
        assertTrue(results.getValue("testStore").status is SyncOk)
        assertEquals(1, results.size)

        `when`(testStore.sync(any())).thenReturn(SyncError(Exception("test")))
        results = feature.sync(mockAccount)
        var error = results.getValue("testStore").status
        assertTrue(error is SyncError)
        assertEquals("test", (error as SyncError).exception.message)
        assertEquals(1, results.size)

        // Multiple stores, different result types.
        val anotherStore: SyncableStore<AuthInfo> = mock()
        val anotherFeature = StorageSync(mapOf(
                Pair("testStore", testStore),
                Pair("goodStore", anotherStore)),
                "sync-scope"
        ) { it }

        `when`(anotherStore.sync(any())).thenReturn(SyncOk)

        results = anotherFeature.sync(mockAccount)
        assertEquals(2, results.size)
        error = results.getValue("testStore").status
        assertTrue(error is SyncError)
        assertEquals("test", (error as SyncError).exception.message)
        assertTrue(results.getValue("goodStore").status is SyncOk)
    }

    @Test
    fun `sync status can be observed`() = runBlocking {
        val mockAccount: OAuthAccount = mock()
        `when`(mockAccount.getTokenServerEndpointURL()).thenReturn("dummyUrl")

        val mockAccessTokenInfo = AccessTokenInfo(
                scope = "scope", key = OAuthScopedKey("kty", "scope", "kid", "k"), token = "token", expiresAt = 0
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
        val testStore = object : SyncableStore<AuthInfo> {
            override suspend fun sync(authInfo: AuthInfo): SyncStatus {
                verifier.verify()
                return SyncOk
            }
        }
        val syncStatusObserver: SyncStatusObserver = mock()

        val feature = StorageSync(mapOf("testStore" to testStore), "sync-scope") { it }

        // These assertions will run while sync is in progress.
        verifier.addVerifyBlock {
            assertTrue(feature.syncMutex.isLocked)
            verify(syncStatusObserver, times(1)).onStarted()
            verify(syncStatusObserver, never()).onIdle()
        }

        feature.register(syncStatusObserver)
        verify(syncStatusObserver, never()).onStarted()
        verify(syncStatusObserver, never()).onIdle()
        assertFalse(feature.syncMutex.isLocked)

        feature.sync(mockAccount)

        verify(syncStatusObserver, times(1)).onStarted()
        verify(syncStatusObserver, times(1)).onIdle()
        assertFalse(feature.syncMutex.isLocked)
    }
}
