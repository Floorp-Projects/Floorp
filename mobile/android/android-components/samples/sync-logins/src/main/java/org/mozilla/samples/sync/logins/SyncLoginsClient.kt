package org.mozilla.samples.sync.logins

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Deferred
import mozilla.appservices.logins.ServerPassword
import mozilla.appservices.logins.SyncUnlockInfo
import mozilla.components.service.fxa.OAuthInfo
import mozilla.components.service.sync.logins.AsyncLoginsStorage
import mozilla.components.service.sync.logins.AsyncLoginsStorageAdapter

/**
 * A minimal login example.
 *
 * Note that this code likely has issues if accessed by multiple threads (especially around
 * initialization and lock/unlock).
 */
class SyncLoginsClient(databasePath: String) : AutoCloseable {
    // Cached unlocked instance of LoginsStorage.
    private var store: AsyncLoginsStorage =
            AsyncLoginsStorageAdapter.forDatabase(databasePath)

    override fun close() {
        store.close()
    }

    /**
     * Perform a sync, and then get the local password list. Unlocks the store if necessary. Note
     * that this may reject with a [SyncAuthInvalidException], in which case the oauth token may
     * need to be refreshed.
     *
     * @param oauthInfo This should be produced by [FirefoxAccount.completeOAuthFlow], for a flow begun
     * with `wantsKeys = true`, that requested (at least) the scopes
     * `https://identity.mozilla.com/apps/oldsync` and `https://identity.mozilla.com/apps/lockbox`.
     *
     * @param tokenServerURL This should be the result of [FirefoxAccount.getTokenServerEndpointURL]
     */
    suspend fun syncAndGetPasswords(oauthInfo: OAuthInfo, tokenServerURL: String): List<ServerPassword> {
        val store = this.getUnlockedStore()
        val unlockInfo = this.getUnlockInfo(oauthInfo, tokenServerURL)
        store.sync(unlockInfo).await()
        return getLocalPasswordList()
    }

    /**
     * Get the local password list (without syncing). Unlocks the store if necessary.
     */
    private suspend fun getLocalPasswordList(): List<ServerPassword> {
        val store = getUnlockedStore()
        return store.list().await()
    }

    private suspend fun getUnlockedStore(): AsyncLoginsStorage {
        if (this.store.isLocked()) {
            this.store.unlock(getSecretKey().await()).await()
        }
        return this.store
    }

    // Helper to convert FxA OAuthInfo + TokenServer URL to `SyncUnlockInfo`.
    private fun getUnlockInfo(oauthInfo: OAuthInfo, tokenServerURL: String): SyncUnlockInfo {
        val keys = oauthInfo.keys ?: throw RuntimeException("keys are missing!")
        val keyInfo = keys["https://identity.mozilla.com/apps/oldsync"]
                ?: throw RuntimeException("Key info is missing!")

        return SyncUnlockInfo(
                kid = keyInfo.kid,
                fxaAccessToken = oauthInfo.accessToken,
                syncKey = keyInfo.k,
                tokenserverURL = tokenServerURL
        )
    }

    // In reality you'd do some (probably asynchronous) thing where you fetch
    // this from some secure storage (or from the user) or whatever, but we just
    // hard-code a key to keep the example simple.
    private fun getSecretKey(): Deferred<String> {
        return CompletableDeferred("my_cool_secret_key")
    }
}
