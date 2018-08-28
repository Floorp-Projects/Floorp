package org.mozilla.samples.sync.logins

import mozilla.components.service.fxa.*
import org.json.JSONObject
import org.mozilla.sync15.logins.*

/**
 * A minimal login example.
 *
 * Note that this code likely has issues if accessed by multiple threads (especially around
 * initialization and lock/unlock).
 */
class SyncLoginsClient(private val databasePath: String) {
    // Cached unlocked instance of LoginsStorage.
    private var unlockedStore: LoginsStorage? = null

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
    fun syncAndGetPasswords(oauthInfo: OAuthInfo, tokenServerURL: String): SyncResult<List<ServerPassword>> {
        return this.getUnlockedStore().then { store ->
            val unlockInfo = this.getUnlockInfo(oauthInfo, tokenServerURL)
            store.sync(unlockInfo)
        }.then {
            this.getLocalPasswordList()
        }
    }

    /**
     * Get the local password list (without syncing). Unlocks the store if necessary.
     */
    fun getLocalPasswordList(): SyncResult<List<ServerPassword>> {
        return this.getUnlockedStore().then { it.list() }
    }

    /**
     * Lock the local store. Returns whether or not the store was previously locked.
     */
    fun lockStore(): SyncResult<Boolean> {
        // Note: this isn't thread safe.
        val store = this.unlockedStore
        this.unlockedStore = null
        if (store == null) {
            return SyncResult.fromValue(false)
        }
        return store.isLocked().then { wasLocked ->
            if (wasLocked) {
                // This shouldn't happen given how we've structured this example, but it's simple
                // to handle, and worth handling in the case that some of this is copy-pasted into
                // situations where this is possible.
                SyncResult.fromValue(false)
            } else {
                store.lock().then { SyncResult.fromValue(true) }
            }
        }
    }

    private fun getUnlockedStore(): SyncResult<LoginsStorage> {
        return if (this.unlockedStore != null) {
            SyncResult.fromValue(this.unlockedStore!!)
        } else {
            // Initialize and unlock the store.
            val store = DatabaseLoginsStorage(this.databasePath)
            return getSecretKey().then { key ->
                store.unlock(key)
            }.then {
                this.unlockedStore = store
                SyncResult.fromValue(store as LoginsStorage)
            }
        }
    }

    // Helper to convert FxA OAuthInfo + TokenServer URL to `SyncUnlockInfo`.
    private fun getUnlockInfo(oauthInfo: OAuthInfo, tokenServerURL: String): SyncUnlockInfo {
        val keys = oauthInfo.keys ?: throw RuntimeException("keys are missing!")
        val keysObj = JSONObject(keys)
        val keyInfo = keysObj.getJSONObject("https://identity.mozilla.com/apps/oldsync") ?: throw RuntimeException("Key info is missing!")

        return SyncUnlockInfo(
                kid            = keyInfo.getString("kid") ?: throw RuntimeException("keyInfo.kid is missing!"),
                fxaAccessToken = oauthInfo.accessToken ?: throw RuntimeException("accessToken is missing!"),
                syncKey        = keyInfo.getString("k") ?: throw RuntimeException("keyInfo.k is missing!"),
                tokenserverURL = tokenServerURL
        )
    }

    // In reality you'd do some (probably asynchronous) thing where you fetch
    // this from some secure storage (or from the user) or whatever, but we just
    // hard-code a key to keep the example simple.
    private fun getSecretKey(): SyncResult<String> {
        return SyncResult.fromValue("my_cool_secret_key")
    }

}
