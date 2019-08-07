/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import kotlinx.coroutines.async
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.plus
import mozilla.appservices.fxaclient.FirefoxAccount as InternalFxAcct

import mozilla.components.concept.sync.AccessTokenInfo
import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import mozilla.components.concept.sync.StatePersistenceCallback
import mozilla.components.support.base.log.logger.Logger

typealias PersistCallback = mozilla.appservices.fxaclient.FirefoxAccount.PersistCallback

/**
 * FirefoxAccount represents the authentication state of a client.
 */
@Suppress("TooManyFunctions")
class FirefoxAccount internal constructor(
    private val inner: InternalFxAcct
) : OAuthAccount {
    private val job = SupervisorJob()
    private val scope = CoroutineScope(Dispatchers.IO) + job

    private val logger = Logger("FirefoxAccount")

    /**
     * Why this exists: in the `init` block below you'll notice that we register a persistence callback
     * as soon as we initialize this object. Essentially, we _always_ have a persistence callback
     * registered with [InternalFxAcct]. However, our own lifecycle is such that we will not know
     * how to actually persist account state until sometime after this object has been created.
     * Currently, we're expecting [FxaAccountManager] to configure a real callback.
     * This wrapper exists to facilitate that flow of events.
     */
    private class WrappingPersistenceCallback : PersistCallback {
        private val logger = Logger("WrappingPersistenceCallback")
        @Volatile
        private var persistenceCallback: StatePersistenceCallback? = null

        fun setCallback(callback: StatePersistenceCallback) {
            logger.debug("Setting persistence callback")
            persistenceCallback = callback
        }

        override fun persist(data: String) {
            val callback = persistenceCallback

            if (callback == null) {
                logger.warn("InternalFxAcct tried persist state, but persistence callback is not set")
            } else {
                logger.debug("Logging state to $callback")
                callback.persist(data)
            }
        }
    }

    private var persistCallback = WrappingPersistenceCallback()
    private val deviceConstellation = FxaDeviceConstellation(inner, scope)

    init {
        inner.registerPersistCallback(persistCallback)
    }

    /**
     * Construct a FirefoxAccount from a [Config], a clientId, and a redirectUri.
     *
     * @param persistCallback This callback will be called every time the [FirefoxAccount]
     * internal state has mutated.
     * The FirefoxAccount instance can be later restored using the
     * [FirefoxAccount.fromJSONString]` class method.
     * It is the responsibility of the consumer to ensure the persisted data
     * is saved in a secure location, as it can contain Sync Keys and
     * OAuth tokens.
     *
     * Note that it is not necessary to `close` the Config if this constructor is used (however
     * doing so will not cause an error).
     */
    constructor(
        config: ServerConfig,
        persistCallback: PersistCallback? = null
    ) : this(InternalFxAcct(config, persistCallback))

    override fun close() {
        deviceConstellation.stopPeriodicRefresh()
        job.cancel()
        inner.close()
    }

    override fun registerPersistenceCallback(callback: mozilla.components.concept.sync.StatePersistenceCallback) {
        persistCallback.setCallback(callback)
    }

    /**
     * Constructs a URL used to begin the OAuth flow for the requested scopes and keys.
     *
     * @param scopes List of OAuth scopes for which the client wants access
     * @param wantsKeys Fetch keys for end-to-end encryption of data from Mozilla-hosted services
     * @return Deferred<String> that resolves to the flow URL when complete
     */
    override fun beginOAuthFlowAsync(scopes: Set<String>, wantsKeys: Boolean): Deferred<String?> {
        return scope.async {
            handleFxaExceptions(logger, "begin oauth flow", { null }) {
                inner.beginOAuthFlow(scopes.toTypedArray(), wantsKeys)
            }
        }
    }

    override fun beginPairingFlowAsync(pairingUrl: String, scopes: Set<String>): Deferred<String?> {
        return scope.async {
            handleFxaExceptions(logger, "begin oauth pairing flow", { null }) {
                inner.beginPairingFlow(pairingUrl, scopes.toTypedArray())
            }
        }
    }

    /**
     * Fetches the profile object for the current client either from the existing cached account,
     * or from the server (requires the client to have access to the profile scope).
     *
     * @param ignoreCache Fetch the profile information directly from the server
     * @return Profile (optional, if successfully retrieved) representing the user's basic profile info
     */
    override fun getProfileAsync(ignoreCache: Boolean): Deferred<Profile?> {
        return scope.async {
            handleFxaExceptions(logger, "getProfile", { null }) {
                inner.getProfile(ignoreCache).into()
            }
        }
    }

    override fun migrateFromSessionTokenAsync(sessionToken: String, kSync: String, kXCS: String): Deferred<Boolean> {
        return scope.async {
            handleFxaExceptions(logger, "migrateFromSessionToken") {
                inner.migrateFromSessionToken(sessionToken, kSync, kXCS)
            }
        }
    }

    /**
     * Convenience method to fetch the profile from a cached account by default, but fall back
     * to retrieval from the server.
     *
     * @return Profile (optional, if successfully retrieved) representing the user's basic profile info
     */
    override fun getProfileAsync(): Deferred<Profile?> = getProfileAsync(false)

    /**
     * Fetches the token server endpoint, for authentication using the SAML bearer flow.
     */
    override fun getTokenServerEndpointURL(): String {
        return inner.getTokenServerEndpointURL()
    }

    /**
     * Fetches the connection success url.
     */
    fun getConnectionSuccessURL(): String {
        return inner.getConnectionSuccessURL()
    }

    /**
     * Authenticates the current account using the code and state parameters fetched from the
     * redirect URL reached after completing the sign in flow triggered by [beginOAuthFlowAsync].
     *
     * Modifies the FirefoxAccount state.
     */
    override fun completeOAuthFlowAsync(code: String, state: String): Deferred<Boolean> {
        return scope.async {
            handleFxaExceptions(logger, "complete oauth flow") {
                inner.completeOAuthFlow(code, state)
            }
        }
    }

    /**
     * Tries to fetch an access token for the given scope.
     *
     * @param singleScope Single OAuth scope (no spaces) for which the client wants access
     * @return [AccessTokenInfo] that stores the token, along with its scope, key and
     *                           expiration timestamp (in seconds) since epoch when complete
     */
    override fun getAccessTokenAsync(singleScope: String): Deferred<AccessTokenInfo?> {
        return scope.async {
            handleFxaExceptions(logger, "get access token", { null }) {
                inner.getAccessToken(singleScope).into()
            }
        }
    }

    /**
     * This method should be called when a request made with an OAuth token failed with an
     * authentication error. It will re-build cached state and perform a connectivity check.
     *
     * This will use the network.
     *
     * In time, fxalib will grow a similar method, at which point we'll just relay to it.
     * See https://github.com/mozilla/application-services/issues/1263
     *
     * @param singleScope An oauth scope for which to check authorization state.
     * @return An optional [Boolean] flag indicating if we're connected, or need to go through
     * re-authentication. A null result means we were not able to determine state at this time.
     */
    override fun checkAuthorizationStatusAsync(singleScope: String): Deferred<Boolean?> {
        // fxalib maintains some internal token caches that need to be cleared whenever we
        // hit an auth problem. Call below makes that clean-up happen.
        inner.clearAccessTokenCache()

        // Now that internal token caches are cleared, we can perform a connectivity check.
        // Do so by requesting a new access token using an internally-stored "refresh token".
        // Success here means that we're still able to connect - our cached access token simply expired.
        // Failure indicates that we need to re-authenticate.
        return scope.async {
            try {
                inner.getAccessToken(singleScope)
                // We were able to obtain a token, so we're in a good authorization state.
                true
            } catch (e: FxaUnauthorizedException) {
                // We got back a 401 while trying to obtain a new access token, which means our refresh
                // token is also in a bad state. We need re-authentication for the tested scope.
                false
            } catch (e: FxaPanicException) {
                // Re-throw any panics we may encounter.
                throw e
            } catch (e: FxaException) {
                // On any other FxaExceptions (networking, etc) we have to return an indeterminate result.
                null
            }
            // Re-throw all other exceptions.
        }
    }

    /**
     * Reset internal account state and destroy current device record.
     * Use this when device record is no longer relevant, e.g. while logging out. On success, other
     * devices will no longer see the current device in their device lists.
     *
     * @return A [Deferred] that will be resolved with a success flag once operation is complete.
     * Failure indicates that we may have failed to destroy current device record. Nothing to do for
     * the consumer; device record will be cleaned up eventually via TTL.
     */
    override fun disconnectAsync(): Deferred<Boolean> {
        return scope.async {
            // TODO can this ever throw FxaUnauthorizedException? would that even make sense? or is that a bug?
            handleFxaExceptions(logger, "disconnect", { false }) {
                inner.disconnect()
                true
            }
        }
    }

    override fun deviceConstellation(): DeviceConstellation {
        return deviceConstellation
    }

    /**
     * Saves the current account's authentication state as a JSON string, for persistence in
     * the Android KeyStore/shared preferences. The authentication state can be restored using
     * [FirefoxAccount.fromJSONString].
     *
     * @return String containing the authentication details in JSON format
     */
    override fun toJSONString(): String = inner.toJSONString()

    companion object {
        /**
         * Restores the account's authentication state from a JSON string produced by
         * [FirefoxAccount.toJSONString].
         *
         * @param persistCallback This callback will be called every time the [FirefoxAccount]
         * internal state has mutated.
         * The FirefoxAccount instance can be later restored using the
         * [FirefoxAccount.fromJSONString]` class method.
         * It is the responsibility of the consumer to ensure the persisted data
         * is saved in a secure location, as it can contain Sync Keys and
         * OAuth tokens.
         *
         * @return [FirefoxAccount] representing the authentication state
         */
        fun fromJSONString(json: String, persistCallback: PersistCallback? = null): FirefoxAccount {
            return FirefoxAccount(InternalFxAcct.fromJSONString(json, persistCallback))
        }
    }
}
