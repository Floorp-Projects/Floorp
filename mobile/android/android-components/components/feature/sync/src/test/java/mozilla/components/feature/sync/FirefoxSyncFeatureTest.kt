/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sync

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.storage.SyncError
import mozilla.components.concept.storage.SyncOk
import mozilla.components.concept.storage.SyncableStore
import mozilla.components.service.fxa.AccessTokenInfo
import mozilla.components.service.fxa.FirefoxAccountShaped
import mozilla.components.service.fxa.OAuthScopedKey
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.Assert.assertTrue
import org.mockito.Mockito.`when`
import java.lang.Exception
import java.util.concurrent.Executors

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

    private val testContext = Executors.newSingleThreadExecutor().asCoroutineDispatcher()

    @Test
    fun `sync with no stores`() = runBlocking {
        val feature = FirefoxSyncFeature(testContext) { it.into() }
        val results = feature.sync(mock()).await()
        assertTrue(results.isEmpty())
    }

    @Test
    fun `sync with configured stores`() = runBlocking {
        val feature = FirefoxSyncFeature(testContext) { it.into() }

        val mockAccount: FirefoxAccountShaped = mock()
        `when`(mockAccount.getTokenServerEndpointURL()).thenReturn("dummyUrl")

        val mockAccessTokenInfo = AccessTokenInfo(
            key = OAuthScopedKey("kid", "k"), token = "token", expiresAt = 0
        )
        `when`(mockAccount.getAccessToken(any())).thenReturn(CompletableDeferred((mockAccessTokenInfo)))

        // Single store, different result types.
        val testStore: SyncableStore<TestAuthType> = mock()
        feature.addSyncable("testStore", testStore)

        `when`(testStore.sync(any())).thenReturn(SyncOk)
        var results = feature.sync(mockAccount).await()
        assertTrue(results["testStore"]!!.status is SyncOk)
        assertEquals(1, results.size)

        `when`(testStore.sync(any())).thenReturn(SyncError(Exception("test")))
        results = feature.sync(mockAccount).await()
        var error = results["testStore"]!!.status
        assertTrue(error is SyncError)
        assertEquals("test", (error as SyncError).exception.message)
        assertEquals(1, results.size)

        // Multiple stores, different result types.
        val anotherStore: SyncableStore<TestAuthType> = mock()
        feature.addSyncable("goodStore", anotherStore)

        `when`(anotherStore.sync(any())).thenReturn(SyncOk)

        results = feature.sync(mockAccount).await()
        assertEquals(2, results.size)
        error = results["testStore"]!!.status
        assertTrue(error is SyncError)
        assertEquals("test", (error as SyncError).exception.message)
        assertTrue(results["goodStore"]!!.status is SyncOk)
    }
}
