/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.manager

import android.content.Context
import androidx.annotation.GuardedBy
import androidx.annotation.VisibleForTesting
import androidx.lifecycle.LifecycleOwner
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import kotlinx.coroutines.cancel
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthException
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.DeviceEvent
import mozilla.components.concept.sync.DeviceEventsObserver
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import mozilla.components.concept.sync.StatePersistenceCallback
import mozilla.components.service.fxa.AccountStorage
import mozilla.components.service.fxa.DeviceConfig
import mozilla.components.service.fxa.FirefoxAccount
import mozilla.components.service.fxa.FxaException
import mozilla.components.service.fxa.FxaPanicException
import mozilla.components.service.fxa.ServerConfig
import mozilla.components.service.fxa.SharedPrefAccountStorage
import mozilla.components.service.fxa.SyncAuthInfoCache
import mozilla.components.service.fxa.SyncConfig
import mozilla.components.service.fxa.asSyncAuthInfo
import mozilla.components.service.fxa.sharing.AccountSharing
import mozilla.components.service.fxa.sharing.ShareableAccount
import mozilla.components.service.fxa.sync.SyncManager
import mozilla.components.service.fxa.sync.SyncStatusObserver
import mozilla.components.service.fxa.sync.WorkManagerSyncManager
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import java.io.Closeable
import java.lang.Exception
import java.lang.IllegalArgumentException
import java.lang.ref.WeakReference
import java.util.concurrent.ConcurrentLinkedQueue
import java.util.concurrent.Executors
import kotlin.coroutines.CoroutineContext

/**
 * A global registry for propagating [AuthException] errors. Components such as [SyncManager] and
 * [FxaDeviceRefreshManager] may encounter authentication problems during their normal operation, and
 * this registry is how they inform [FxaAccountManager] that these errors happened.
 *
 * [FxaAccountManager] monitors this registry, adjusts internal state accordingly, and notifies
 * registered [AccountObserver] that account needs re-authentication.
 */
val authErrorRegistry = ObserverRegistry<AuthErrorObserver>()

interface AuthErrorObserver {
    fun onAuthErrorAsync(e: AuthException): Deferred<Unit>
}

/**
 * Observer interface which lets its consumers respond to authentication requests.
 */
private interface OAuthObserver {
    /**
     * Account manager is requesting for an OAUTH flow to begin.
     * @param authUrl Starting point for the OAUTH flow.
     */
    fun onBeginOAuthFlow(authUrl: String)

    /**
     * Account manager encountered an error during authentication.
     */
    fun onError()
}

const val SCOPE_PROFILE = "profile"
const val SCOPE_SYNC = "https://identity.mozilla.com/apps/oldsync"

/**
 * An account manager which encapsulates various internal details of an account lifecycle and provides
 * an observer interface along with a public API for interacting with an account.
 * The internal state machine abstracts over state space as exposed by the fxaclient library, not
 * the internal states experienced by lower-level representation of a Firefox Account; those are opaque to us.
 *
 * Class is 'open' to facilitate testing.
 *
 * @param context A [Context] instance that's used for internal messaging and interacting with local storage.
 * @param serverConfig A [ServerConfig] used for account initialization.
 * @param deviceConfig A description of the current device (name, type, capabilities).
 * @param syncConfig Optional, initial sync behaviour configuration. Sync will be disabled if this is `null`.
 * @param applicationScopes A set of scopes which will be requested during account authentication.
 */
@Suppress("TooManyFunctions", "LargeClass")
open class FxaAccountManager(
    private val context: Context,
    private val serverConfig: ServerConfig,
    private val deviceConfig: DeviceConfig,
    @Volatile private var syncConfig: SyncConfig?,
    private val applicationScopes: Set<String> = emptySet(),
    // We want a single-threaded execution model for our account-related "actions" (state machine side-effects).
    // That is, we want to ensure a sequential execution flow, but on a background thread.
    private val coroutineContext: CoroutineContext = Executors
        .newSingleThreadExecutor().asCoroutineDispatcher() + SupervisorJob()
) : Closeable, Observable<AccountObserver> by ObserverRegistry() {
    private val logger = Logger("FirefoxAccountStateMachine")

    // This is used during 'beginAuthenticationAsync' call, which returns a Deferred<String>.
    // 'deferredAuthUrl' is set on this observer and returned, and resolved once state machine goes
    // through its motions. This allows us to keep around only one observer in the registry.
    private val fxaOAuthObserver = FxaOAuthObserver()
    private class FxaOAuthObserver : OAuthObserver {
        @Volatile
        lateinit var deferredAuthUrl: CompletableDeferred<String?>

        override fun onBeginOAuthFlow(authUrl: String) {
            deferredAuthUrl.complete(authUrl)
        }

        override fun onError() {
            deferredAuthUrl.complete(null)
        }
    }
    private val oauthObservers = object : Observable<OAuthObserver> by ObserverRegistry() {}
    init {
        oauthObservers.register(fxaOAuthObserver)
    }

    private class FxaAuthErrorObserver(val manager: FxaAccountManager) : AuthErrorObserver {
        override fun onAuthErrorAsync(e: AuthException): Deferred<Unit> {
            return manager.processQueueAsync(Event.AuthenticationError(e))
        }
    }
    private val fxaAuthErrorObserver = FxaAuthErrorObserver(this)

    init {
        authErrorRegistry.register(fxaAuthErrorObserver)
    }

    private class FxaStatePersistenceCallback(
        private val accountManager: WeakReference<FxaAccountManager>
    ) : StatePersistenceCallback {
        private val logger = Logger("FxaStatePersistenceCallback")

        override fun persist(data: String) {
            val manager = accountManager.get()
            logger.debug("Persisting account state into ${manager?.getAccountStorage()}")
            manager?.getAccountStorage()?.write(data)
        }
    }

    private lateinit var statePersistenceCallback: FxaStatePersistenceCallback

    companion object {
        /**
         * State transition matrix. It's in the companion object to enforce purity.
         * @return An optional [AccountState] if provided state+event combination results in a
         * state transition. Note that states may transition into themselves.
         */
        internal fun nextState(state: AccountState, event: Event): AccountState? =
            when (state) {
                AccountState.Start -> when (event) {
                    Event.Init -> AccountState.Start
                    Event.AccountNotFound -> AccountState.NotAuthenticated
                    Event.AccountRestored -> AccountState.AuthenticatedNoProfile
                    else -> null
                }
                AccountState.NotAuthenticated -> when (event) {
                    Event.Authenticate -> AccountState.NotAuthenticated
                    Event.FailedToAuthenticate -> AccountState.NotAuthenticated

                    is Event.SignInShareableAccount -> AccountState.NotAuthenticated
                    Event.SignedInShareableAccount -> AccountState.AuthenticatedNoProfile

                    is Event.Pair -> AccountState.NotAuthenticated
                    is Event.Authenticated -> AccountState.AuthenticatedNoProfile
                    else -> null
                }
                AccountState.AuthenticatedNoProfile -> when (event) {
                    is Event.AuthenticationError -> AccountState.AuthenticationProblem
                    Event.FetchProfile -> AccountState.AuthenticatedNoProfile
                    Event.FetchedProfile -> AccountState.AuthenticatedWithProfile
                    Event.FailedToFetchProfile -> AccountState.AuthenticatedNoProfile
                    Event.Logout -> AccountState.NotAuthenticated
                    else -> null
                }
                AccountState.AuthenticatedWithProfile -> when (event) {
                    is Event.AuthenticationError -> AccountState.AuthenticationProblem
                    Event.Logout -> AccountState.NotAuthenticated
                    else -> null
                }
                AccountState.AuthenticationProblem -> when (event) {
                    Event.Authenticate -> AccountState.AuthenticationProblem
                    Event.FailedToAuthenticate -> AccountState.AuthenticationProblem
                    Event.RecoveredFromAuthenticationProblem -> AccountState.AuthenticatedNoProfile
                    is Event.Authenticated -> AccountState.AuthenticatedNoProfile
                    Event.Logout -> AccountState.NotAuthenticated
                    else -> null
                }
        }
    }

    // 'account' is initialized during processing of an 'Init' event.
    // Note on threading: we use a single-threaded executor, so there's no concurrent access possible.
    // However, that executor doesn't guarantee that it'll always use the same thread, and so vars
    // are marked as volatile for across-thread visibility. Similarly, event queue uses a concurrent
    // list, although that's probably an overkill.
    @Volatile private lateinit var account: OAuthAccount
    @Volatile private var profile: Profile? = null
    @Volatile private var state = AccountState.Start
    private val eventQueue = ConcurrentLinkedQueue<Event>()

    private val deviceEventObserverRegistry = ObserverRegistry<DeviceEventsObserver>()
    private val deviceEventsIntegration = DeviceEventsIntegration(deviceEventObserverRegistry)

    private val syncStatusObserverRegistry = ObserverRegistry<SyncStatusObserver>()

    // We always obtain a "profile" scope, as that's assumed to be needed for any application integration.
    // We obtain a sync scope only if this was requested by the application via SyncConfig.
    // Additionally, we obtain any scopes that application requested explicitly.
    private val scopes: Set<String>
        get() = if (syncConfig != null) {
            setOf(SCOPE_PROFILE, SCOPE_SYNC)
        } else {
            setOf(SCOPE_PROFILE)
        }.plus(applicationScopes)

    // Internal backing field for the syncManager. This will be `null` if passed in SyncConfig (either
    // via the constructor, or via [setSyncConfig]) is also `null` - that is, sync will be disabled.
    // Note that trying to perform a sync while account isn't authenticated will not succeed.
    @GuardedBy("this")
    private var syncManager: SyncManager? = null
    private var syncManagerIntegration: AccountsToSyncIntegration? = null
    private val internalSyncStatusObserver = SyncToAccountsIntegration(this)

    init {
        syncConfig?.let { setSyncConfigAsync(it) }

        if (syncManager == null) {
            logger.info("Sync is disabled")
        } else {
            logger.info("Sync is enabled")
        }
    }

    /**
     * Queries trusted FxA Auth providers available on the device, returning a list of [ShareableAccount]
     * in an order of preference. Any of the returned [ShareableAccount] may be used with
     * [signInWithShareableAccountAsync] to sign-in into an FxA account without any required user input.
     */
    fun shareableAccounts(context: Context): List<ShareableAccount> {
        return AccountSharing.queryShareableAccounts(context)
    }

    /**
     * Uses a provided [fromAccount] to sign-in into a corresponding FxA account without any required
     * user input. Once sign-in completes, any registered [AccountObserver.onAuthenticated] listeners
     * will be notified and [authenticatedAccount] will refer to the new account.
     * This may fail in case of network errors, or if provided credentials are not valid.
     * @return A deferred boolean flag indicating success (if true) of the sign-in operation.
     */
    fun signInWithShareableAccountAsync(fromAccount: ShareableAccount): Deferred<Boolean> {
        val stateMachineTransition = processQueueAsync(Event.SignInShareableAccount(fromAccount))
        val result = CompletableDeferred<Boolean>()
        CoroutineScope(coroutineContext).launch {
            stateMachineTransition.await()
            result.complete(authenticatedAccount() != null)
        }
        return result
    }

    /**
     * Allows setting a new [SyncConfig], changing sync behaviour.
     *
     * @param config Sync behaviour configuration, see [SyncConfig].
     */
    fun setSyncConfigAsync(config: SyncConfig): Deferred<Unit> = synchronized(this) {
        logger.info("Enabling/updating sync with a new SyncConfig: $config")

        syncConfig = config
        // Let the existing manager, if it's present, clean up (cancel periodic syncing, etc).
        syncManager?.stop()
        syncManagerIntegration = null

        val result = CompletableDeferred<Unit>()

        // Initialize a new sync manager with the passed-in config.
        if (config.syncableStores.isEmpty()) {
            throw IllegalArgumentException("Set of stores can't be empty")
        }

        syncManager = createSyncManager(config).also { manager ->
            // Observe account state changes.
            syncManagerIntegration = AccountsToSyncIntegration(manager).also { accountToSync ->
                this@FxaAccountManager.register(accountToSync)
            }
            // Observe sync status changes.
            manager.registerSyncStatusObserver(internalSyncStatusObserver)
            // If account is currently authenticated, 'enable' the sync manager.
            if (authenticatedAccount() != null) {
                CoroutineScope(coroutineContext).launch {
                    // Make sure auth-info cache is populated before starting sync manager.
                    maybeUpdateSyncAuthInfoCache()
                    manager.start()
                    result.complete(Unit)
                }
            } else {
                result.complete(Unit)
            }
        }

        return result
    }

    /**
     * Request an immediate synchronization, as configured according to [syncConfig].
     *
     * @param startup Boolean flag indicating if sync is being requested in a startup situation.
     * @param debounce Boolean flag indicating if this sync may be debounced (in case another sync executed recently).
     */
    fun syncNowAsync(
        startup: Boolean = false,
        debounce: Boolean = false
    ): Deferred<Unit> = CoroutineScope(coroutineContext).async {
        // Make sure auth cache is populated before we try to sync.
        maybeUpdateSyncAuthInfoCache()

        // Access to syncManager is guarded by `this`.
        synchronized(this) {
            if (syncManager == null) {
                throw IllegalStateException(
                        "Sync is not configured. Construct this class with a 'syncConfig' or use 'setSyncConfig'"
                )
            }
            syncManager?.now(startup, debounce)
        }
        Unit
    }

    /**
     * Indicates if sync is currently running.
     */
    fun isSyncActive() = syncManager?.isSyncActive() ?: false

    /**
     * Call this after registering your observers, and before interacting with this class.
     */
    fun initAsync(): Deferred<Unit> {
        statePersistenceCallback = FxaStatePersistenceCallback(WeakReference(this))
        return processQueueAsync(Event.Init)
    }

    /**
     * Main point for interaction with an [OAuthAccount] instance.
     * @return [OAuthAccount] if we're in an authenticated state, null otherwise. Returned [OAuthAccount]
     * may need to be re-authenticated; consumers are expected to check [accountNeedsReauth].
     */
    fun authenticatedAccount(): OAuthAccount? {
        return when (state) {
            AccountState.AuthenticatedWithProfile,
            AccountState.AuthenticatedNoProfile,
            AccountState.AuthenticationProblem -> account
            else -> null
        }
    }

    /**
     * Indicates if account needs to be re-authenticated via [beginAuthenticationAsync].
     * Most common reason for an account to need re-authentication is a password change.
     *
     * TODO this may return a false-positive, if we're currently going through a recovery flow.
     * Prefer to be notified of auth problems via [AccountObserver], which is reliable.
     *
     * @return A boolean flag indicating if account needs to be re-authenticated.
     */
    fun accountNeedsReauth(): Boolean {
        return when (state) {
            AccountState.AuthenticationProblem -> true
            else -> false
        }
    }

    fun accountProfile(): Profile? {
        return when (state) {
            AccountState.AuthenticatedWithProfile,
            AccountState.AuthenticationProblem -> profile
            else -> null
        }
    }

    fun updateProfileAsync(): Deferred<Unit> {
        return processQueueAsync(Event.FetchProfile)
    }

    fun beginAuthenticationAsync(pairingUrl: String? = null): Deferred<String?> {
        val deferredAuthUrl = CompletableDeferred<String?>()

        // Observer will 'complete' this deferred once state machine resolves its events.
        fxaOAuthObserver.deferredAuthUrl = deferredAuthUrl

        val event = if (pairingUrl != null) Event.Pair(pairingUrl) else Event.Authenticate
        processQueueAsync(event)
        return deferredAuthUrl
    }

    fun finishAuthenticationAsync(code: String, state: String): Deferred<Unit> {
        return processQueueAsync(Event.Authenticated(code, state))
    }

    fun logoutAsync(): Deferred<Unit> {
        return processQueueAsync(Event.Logout)
    }

    fun registerForDeviceEvents(observer: DeviceEventsObserver, owner: LifecycleOwner, autoPause: Boolean) {
        deviceEventObserverRegistry.register(observer, owner, autoPause)
    }

    fun registerForSyncEvents(observer: SyncStatusObserver, owner: LifecycleOwner, autoPause: Boolean) {
        syncStatusObserverRegistry.register(observer, owner, autoPause)
    }

    override fun close() {
        coroutineContext.cancel()
        account.close()
    }

    /**
     * Pumps the state machine until all events are processed and their side-effects resolve.
     */
    private fun processQueueAsync(event: Event): Deferred<Unit> = CoroutineScope(coroutineContext).async {
        eventQueue.add(event)
        do {
            val toProcess: Event = eventQueue.poll()!!
            val transitionInto = nextState(state, toProcess)

            if (transitionInto == null) {
                logger.warn("Got invalid event $toProcess for state $state.")
                continue
            }

            logger.info("Processing event $toProcess for state $state. Next state is $transitionInto")

            state = transitionInto

            stateActions(state, toProcess)?.let { successiveEvent ->
                logger.info("Ran '$toProcess' side-effects for state $state, got successive event $successiveEvent")
                eventQueue.add(successiveEvent)
            }
        } while (!eventQueue.isEmpty())
    }

    /**
     * Side-effects matrix. Defines non-pure operations that must take place for state+event combinations.
     */
    @Suppress("ComplexMethod", "ReturnCount", "ThrowsCount")
    private suspend fun stateActions(forState: AccountState, via: Event): Event? {
        // We're about to enter a new state ('forState') via some event ('via').
        // States will have certain side-effects associated with different event transitions.
        // In other words, the same state may have different side-effects depending on the event
        // which caused a transition.
        // For example, a "NotAuthenticated" state may be entered after a logoutAsync, and its side-effects
        // will include clean-up and re-initialization of an account. Alternatively, it may be entered
        // after we've checked local disk, and didn't find a persisted authenticated account.
        return when (forState) {
            AccountState.Start -> {
                when (via) {
                    Event.Init -> {
                        // Locally corrupt accounts are simply treated as 'absent'.
                        val savedAccount = try {
                            getAccountStorage().read()
                        } catch (e: FxaPanicException) {
                            // Don't swallow panics from the underlying library.
                            throw e
                        } catch (e: FxaException) {
                            logger.error("Failed to load saved account. Re-initializing...", e)
                            null
                        }

                        if (savedAccount == null) {
                            Event.AccountNotFound
                        } else {
                            account = savedAccount
                            Event.AccountRestored
                        }
                    }
                    else -> null
                }
            }
            AccountState.NotAuthenticated -> {
                when (via) {
                    Event.Logout -> {
                        // Clean up internal account state and destroy the current FxA device record.
                        if (account.disconnectAsync().await()) {
                            logger.info("Disconnected FxA account")
                        } else {
                            logger.warn("Failed to fully disconnect the FxA account")
                        }
                        // Clean up resources.
                        profile = null
                        account.close()
                        // Delete persisted state.
                        getAccountStorage().clear()
                        SyncAuthInfoCache(context).clear()
                        // Re-initialize account.
                        account = createAccount(serverConfig)

                        notifyObservers { onLoggedOut() }

                        null
                    }
                    Event.AccountNotFound -> {
                        account = createAccount(serverConfig)

                        null
                    }
                    Event.Authenticate -> {
                        return doAuthenticate()
                    }
                    is Event.SignInShareableAccount -> {
                        account.registerPersistenceCallback(statePersistenceCallback)

                        val migrationResult = account.migrateFromSessionTokenAsync(
                            via.account.authInfo.sessionToken,
                            via.account.authInfo.kSync,
                            via.account.authInfo.kXCS
                        ).await()

                        return if (migrationResult) {
                            Event.SignedInShareableAccount
                        } else {
                            null
                        }
                    }
                    is Event.Pair -> {
                        val url = account.beginPairingFlowAsync(via.pairingUrl, scopes).await()
                        if (url == null) {
                            oauthObservers.notifyObservers { onError() }
                            return Event.FailedToAuthenticate
                        }
                        oauthObservers.notifyObservers { onBeginOAuthFlow(url) }
                        null
                    }
                    else -> null
                }
            }
            AccountState.AuthenticatedNoProfile -> {
                when (via) {
                    is Event.Authenticated -> {
                        logger.info("Registering persistence callback")
                        account.registerPersistenceCallback(statePersistenceCallback)

                        logger.info("Completing oauth flow")
                        account.completeOAuthFlowAsync(via.code, via.state).await()

                        logger.info("Registering device constellation observer")
                        account.deviceConstellation().register(deviceEventsIntegration)

                        logger.info("Initializing device")
                        // NB: underlying API is expected to 'ensureCapabilities' as part of device initialization.
                        account.deviceConstellation().initDeviceAsync(
                            deviceConfig.name, deviceConfig.type, deviceConfig.capabilities
                        ).await()

                        postAuthenticated(true)

                        Event.FetchProfile
                    }
                    Event.AccountRestored -> {
                        logger.info("Registering persistence callback")
                        account.registerPersistenceCallback(statePersistenceCallback)

                        logger.info("Registering device constellation observer")
                        account.deviceConstellation().register(deviceEventsIntegration)

                        // If this is the first time ensuring our capabilities,
                        logger.info("Ensuring device capabilities...")
                        if (account.deviceConstellation().ensureCapabilitiesAsync(deviceConfig.capabilities).await()) {
                            logger.info("Successfully ensured device capabilities.")
                        } else {
                            logger.warn("Failed to ensure device capabilities.")
                        }

                        // We used to perform a periodic device event polling, but for now we do not.
                        // This cancels any periodic jobs device may still have active.
                        // See https://github.com/mozilla-mobile/android-components/issues/3433
                        logger.info("Stopping periodic refresh of the device constellation")
                        account.deviceConstellation().stopPeriodicRefresh()

                        postAuthenticated(false)

                        Event.FetchProfile
                    }
                    Event.SignedInShareableAccount -> {
                        // Note that we are not registering an account persistence callback here like
                        // we do in other `AuthenticatedNoProfile` methods, because it would have been
                        // already registered while handling `Event.SignInShareableAccount`.
                        logger.info("Registering device constellation observer")
                        account.deviceConstellation().register(deviceEventsIntegration)

                        logger.info("Initializing device")
                        // NB: underlying API is expected to 'ensureCapabilities' as part of device initialization.
                        account.deviceConstellation().initDeviceAsync(
                                deviceConfig.name, deviceConfig.type, deviceConfig.capabilities
                        ).await()

                        postAuthenticated(true)

                        Event.FetchProfile
                    }

                    Event.RecoveredFromAuthenticationProblem -> {
                        // This path is a blend of "authenticated" and "account restored".
                        // We need to re-initialize an fxa device, but we don't need to complete an auth.
                        logger.info("Registering persistence callback")
                        account.registerPersistenceCallback(statePersistenceCallback)

                        logger.info("Registering device constellation observer")
                        account.deviceConstellation().register(deviceEventsIntegration)

                        logger.info("Initializing device")
                        // NB: underlying API is expected to 'ensureCapabilities' as part of device initialization.
                        account.deviceConstellation().initDeviceAsync(
                                deviceConfig.name, deviceConfig.type, deviceConfig.capabilities
                        ).await()

                        postAuthenticated(false)

                        Event.FetchProfile
                    }
                    Event.FetchProfile -> {
                        // Profile fetching and account authentication issues:
                        // https://github.com/mozilla/application-services/issues/483
                        logger.info("Fetching profile...")

                        profile = account.getProfileAsync(true).await()
                        if (profile == null) {
                            return Event.FailedToFetchProfile
                        }

                        Event.FetchedProfile
                    }
                    else -> null
                }
            }
            AccountState.AuthenticatedWithProfile -> {
                when (via) {
                    Event.FetchedProfile -> {
                        notifyObservers {
                            onProfileUpdated(profile!!)
                        }
                        null
                    }
                    else -> null
                }
            }
            AccountState.AuthenticationProblem -> {
                when (via) {
                    Event.Authenticate -> {
                        return doAuthenticate()
                    }
                    is Event.AuthenticationError -> {
                        // Somewhere in the system, we've just hit an authentication problem.
                        // There are two main causes:
                        // 1) an access token we've obtain from fxalib via 'getAccessToken' expired
                        // 2) password was changed, or device was revoked
                        // We can recover from (1) and test if we're in (2) by asking the fxalib
                        // to give us a new access token. If it succeeds, then we can go back to whatever
                        // state we were in before. Future operations that involve access tokens should
                        // succeed.
                        // If we fail with a 401, then we know we have a legitimate authentication problem.
                        logger.info("Hit auth problem. Trying to recover.")

                        // Clear our access token cache; it'll be re-populated as part of the
                        // regular state machine flow if we manage to recover.
                        SyncAuthInfoCache(context).clear()

                        // We request an access token for a "profile" scope since that's the only
                        // scope we're guaranteed to have at this point. That is, we don't rely on
                        // passed-in application-specific scopes.
                        when (account.checkAuthorizationStatusAsync(SCOPE_PROFILE).await()) {
                            true -> {
                                logger.info("Able to recover from an auth problem.")

                                // And now we can go back to our regular programming.
                                return Event.RecoveredFromAuthenticationProblem
                            }
                            null, false -> {
                                // We are either certainly in the scenario (2), or were unable to determine
                                // our connectivity state. Let's assume we need to re-authenticate.
                                // This uncertainty about real state means that, hopefully rarely,
                                // we will disconnect users that hit transient network errors during
                                // an authorization check.
                                // See https://github.com/mozilla-mobile/android-components/issues/3347
                                logger.info("Unable to recover from an auth problem, clearing state.")

                                // We perform similar actions to what we do on logout, except for destroying
                                // the device. We don't have valid access tokens at this point to do that!

                                // Clean up resources.
                                profile = null
                                account.close()
                                // Delete persisted state.
                                getAccountStorage().clear()
                                // Re-initialize account.
                                account = createAccount(serverConfig)

                                // Finally, tell our listeners we're in a bad auth state.
                                notifyObservers { onAuthenticationProblems() }
                            }
                        }

                        null
                    }
                    else -> null
                }
            }
        }
    }

    private suspend fun maybeUpdateSyncAuthInfoCache() {
        // Update cached sync auth info only if we have a syncConfig (e.g. sync is enabled)...
        if (syncConfig == null) {
            return
        }

        // .. and our cache is stale.
        val cache = SyncAuthInfoCache(context)
        if (!cache.expired()) {
            return
        }

        // NB: this call will inform authErrorRegistry in case an auth error is encountered.
        account.getAccessTokenAsync(SCOPE_SYNC).await()?.let {
            SyncAuthInfoCache(context).setToCache(
                it.asSyncAuthInfo(account.getTokenServerEndpointURL())
            )
        }
    }

    private suspend fun doAuthenticate(): Event? {
        val url = account.beginOAuthFlowAsync(scopes).await()
        if (url == null) {
            oauthObservers.notifyObservers { onError() }
            return Event.FailedToAuthenticate
        }
        oauthObservers.notifyObservers { onBeginOAuthFlow(url) }
        return null
    }

    private suspend fun postAuthenticated(newAccount: Boolean) {
        // Before any sync workers have a chance to access it, make sure our SyncAuthInfo cache is hot.
        maybeUpdateSyncAuthInfoCache()

        // Notify our internal (sync) and external (app logic) observers.
        notifyObservers { onAuthenticated(account, newAccount) }

        // If device supports SEND_TAB...
        if (deviceConfig.capabilities.contains(DeviceCapability.SEND_TAB)) {
            // ... update constellation state, and poll for any pending device events.
            account.deviceConstellation().refreshDeviceStateAsync().await()
        }
    }

    @VisibleForTesting
    open fun createAccount(config: ServerConfig): OAuthAccount {
        return FirefoxAccount(config)
    }

    @VisibleForTesting
    open fun createSyncManager(config: SyncConfig): SyncManager {
        return WorkManagerSyncManager(config)
    }

    @VisibleForTesting
    open fun getAccountStorage(): AccountStorage {
        return SharedPrefAccountStorage(context)
    }

    /**
     * In the future, this could be an internal account-related events processing layer.
     * E.g., once we grow events such as "please logout".
     * For now, we just pass everything downstream as-is.
     */
    private class DeviceEventsIntegration(
        private val listenerRegistry: ObserverRegistry<DeviceEventsObserver>
    ) : DeviceEventsObserver {
        private val logger = Logger("DeviceEventsIntegration")

        override fun onEvents(events: List<DeviceEvent>) {
            logger.info("Received events, notifying listeners")
            listenerRegistry.notifyObservers { onEvents(events) }
        }
    }

    /**
     * Account status events flowing into the sync manager.
     */
    private class AccountsToSyncIntegration(
        private val syncManager: SyncManager
    ) : AccountObserver {
        override fun onLoggedOut() {
            syncManager.stop()
        }

        override fun onAuthenticated(account: OAuthAccount, newAccount: Boolean) {
            syncManager.start()
        }

        override fun onProfileUpdated(profile: Profile) {
            // Sync currently doesn't care about the FxA profile.
            // In the future, we might kick-off an immediate sync here.
        }

        override fun onAuthenticationProblems() {
            syncManager.stop()
        }
    }

    /**
     * Sync status changes flowing into account manager.
     */
    private class SyncToAccountsIntegration(
        private val accountManager: FxaAccountManager
    ) : SyncStatusObserver {
        override fun onStarted() {
            accountManager.syncStatusObserverRegistry.notifyObservers { onStarted() }
        }

        override fun onIdle() {
            accountManager.syncStatusObserverRegistry.notifyObservers { onIdle() }
        }

        override fun onError(error: Exception?) {
            accountManager.syncStatusObserverRegistry.notifyObservers { onError(error) }
        }
    }
}
