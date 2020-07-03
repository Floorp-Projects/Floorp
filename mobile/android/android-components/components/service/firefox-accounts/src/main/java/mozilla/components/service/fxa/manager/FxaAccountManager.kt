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
import kotlinx.coroutines.withContext
import mozilla.appservices.syncmanager.DeviceSettings
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.AuthFlowUrl
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.AccountEvent
import mozilla.components.concept.sync.AccountEventsObserver
import mozilla.components.concept.sync.InFlightMigrationState
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import mozilla.components.concept.sync.StatePersistenceCallback
import mozilla.components.service.fxa.AccountManagerException
import mozilla.components.service.fxa.AccountStorage
import mozilla.components.service.fxa.DeviceConfig
import mozilla.components.service.fxa.FxaDeviceSettingsCache
import mozilla.components.service.fxa.FirefoxAccount
import mozilla.components.service.fxa.FxaAuthData
import mozilla.components.service.fxa.FxaException
import mozilla.components.service.fxa.FxaPanicException
import mozilla.components.service.fxa.SecureAbove22AccountStorage
import mozilla.components.service.fxa.ServerConfig
import mozilla.components.service.fxa.SharedPrefAccountStorage
import mozilla.components.service.fxa.SyncAuthInfoCache
import mozilla.components.service.fxa.SyncConfig
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.asSyncAuthInfo
import mozilla.components.service.fxa.intoSyncType
import mozilla.components.service.fxa.sharing.AccountSharing
import mozilla.components.service.fxa.sharing.ShareableAccount
import mozilla.components.service.fxa.sync.SyncManager
import mozilla.components.service.fxa.sync.SyncReason
import mozilla.components.service.fxa.sync.SyncStatusObserver
import mozilla.components.service.fxa.sync.WorkManagerSyncManager
import mozilla.components.service.fxa.sync.clearSyncState
import mozilla.components.support.base.crash.CrashReporting
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
 * Observer interface which lets its consumers respond to authentication requests.
 */
private interface OAuthObserver {
    /**
     * Account manager is requesting for an OAUTH flow to begin.
     * @param authUrl Starting point for the OAUTH flow.
     */
    fun onBeginOAuthFlow(authFlowUrl: AuthFlowUrl)

    /**
     * Account manager encountered an error during authentication.
     */
    fun onError()
}

// Necessary to fetch a profile.
const val SCOPE_PROFILE = "profile"
// Necessary to obtain sync keys.
const val SCOPE_SYNC = "https://identity.mozilla.com/apps/oldsync"
// Necessary to obtain a sessionToken, which gives full access to the account.
const val SCOPE_SESSION = "https://identity.mozilla.com/tokens/session"

// If we see more than AUTH_CHECK_CIRCUIT_BREAKER_COUNT checks, and each is less than
// AUTH_CHECK_CIRCUIT_BREAKER_RESET_MS since the last check, then we'll trigger a "circuit breaker".
const val AUTH_CHECK_CIRCUIT_BREAKER_RESET_MS = 1000L * 10
const val AUTH_CHECK_CIRCUIT_BREAKER_COUNT = 10
// This logic is in place to protect ourselves from endless auth recovery loops, while at the same
// time allowing for a few 401s to hit the state machine in a quick succession.
// For example, initializing the account state machine & syncing after letting our access tokens expire
// due to long period of inactivity will trigger a few 401s, and that shouldn't be a cause for concern.

/**
 * Describes a result of running [FxaAccountManager.signInWithShareableAccountAsync].
 */
enum class SignInWithShareableAccountResult {
    /**
     * Sign-in failed due to an intermittent problem (such as a network failure). A retry attempt will
     * be performed automatically during account manager initialization, or as a side-effect of certain
     * user actions (e.g. triggering a sync).
     *
     * Applications may treat this account as "authenticated" after seeing this result.
     */
    WillRetry,

    /**
     * Sign-in succeeded with no issues.
     * Applications may treat this account as "authenticated" after seeing this result.
     */
    Success,

    /**
     * Sign-in failed due to non-recoverable issues.
     */
    Failure
}

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
    private val crashReporter: CrashReporting? = null,
    // We want a single-threaded execution model for our account-related "actions" (state machine side-effects).
    // That is, we want to ensure a sequential execution flow, but on a background thread.
    private val coroutineContext: CoroutineContext = Executors
        .newSingleThreadExecutor().asCoroutineDispatcher() + SupervisorJob()
) : Closeable, Observable<AccountObserver> by ObserverRegistry() {
    private val logger = Logger("FirefoxAccountStateMachine")

    @Volatile
    private var latestAuthState: String? = null

    // This is used during 'beginAuthenticationAsync' call, which returns a Deferred<String>.
    // 'deferredAuthUrl' is set on this observer and returned, and resolved once state machine goes
    // through its motions. This allows us to keep around only one observer in the registry.
    private val fxaOAuthObserver = FxaOAuthObserver()
    private class FxaOAuthObserver : OAuthObserver {
        @Volatile
        lateinit var deferredAuthUrl: CompletableDeferred<String?>

        override fun onBeginOAuthFlow(authFlowUrl: AuthFlowUrl) {
            deferredAuthUrl.complete(authFlowUrl.url)
        }

        override fun onError() {
            deferredAuthUrl.complete(null)
        }
    }
    private val oauthObservers = object : Observable<OAuthObserver> by ObserverRegistry() {}
    init {
        oauthObservers.register(fxaOAuthObserver)
    }

    init {
        GlobalAccountManager.setInstance(this)
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

    // 'account' is initialized during processing of an 'Init' event.
    // Note on threading: we use a single-threaded executor, so there's no concurrent access possible.
    // However, that executor doesn't guarantee that it'll always use the same thread, and so vars
    // are marked as volatile for across-thread visibility. Similarly, event queue uses a concurrent
    // list, although that's probably an overkill.
    @Volatile private lateinit var account: OAuthAccount
    @Volatile private var profile: Profile? = null

    // We'd like to persist this state, so that we can short-circuit transition to AuthenticationProblem on
    // initialization, instead of triggering the full state machine knowing in advance we'll hit auth problems.
    // See https://github.com/mozilla-mobile/android-components/issues/5102
    @Volatile private var state = AccountState.Start
    private val eventQueue = ConcurrentLinkedQueue<Event>()

    private val accountEventObserverRegistry = ObserverRegistry<AccountEventsObserver>()
    private val accountEventsIntegration = AccountEventsIntegration(accountEventObserverRegistry)

    private val syncStatusObserverRegistry = ObserverRegistry<SyncStatusObserver>()

    // We always obtain a "profile" scope, as that's assumed to be needed for any application integration.
    // We obtain a sync scope only if this was requested by the application via SyncConfig.
    // Additionally, we obtain any scopes that the application requested explicitly.
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
     * @param reuseSessionToken Whether or not to reuse existing session token (which is part of the [ShareableAccount].
     * @return A deferred boolean flag indicating success (if true) of the sign-in operation.
     */
    fun signInWithShareableAccountAsync(
        fromAccount: ShareableAccount,
        reuseSessionToken: Boolean = false
    ): Deferred<SignInWithShareableAccountResult> {
        val stateMachineTransition = processQueueAsync(Event.SignInShareableAccount(fromAccount, reuseSessionToken))
        val result = CompletableDeferred<SignInWithShareableAccountResult>()
        CoroutineScope(coroutineContext).launch {
            stateMachineTransition.await()

            if (accountMigrationInFlight()) {
                result.complete(SignInWithShareableAccountResult.WillRetry)
            } else if (authenticatedAccount() != null) {
                result.complete(SignInWithShareableAccountResult.Success)
            } else {
                result.complete(SignInWithShareableAccountResult.Failure)
            }
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
        if (config.supportedEngines.isEmpty()) {
            throw IllegalArgumentException("Set of supported engines can't be empty")
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

                    // We enabled syncing for an authenticated account, and we now need to kick-off sync.
                    // How do we know if this is going to be a first sync for an account?
                    // We can make two assumptions, that are probably roughly correct most of the time:
                    // - we know what the user data choices are
                    //  - they were presented with CWTS ("choose what to sync") during sign-up/sign-in
                    // - ... or we're enabling sync for an existing account, with data choices already
                    //   recorded on some other client, e.g. desktop.
                    // In either case, we need to behave as if we're in a "first sync":
                    // - persist local choice, if we've signed for an account up locally and have CWTS
                    // data from the user
                    // - or fetch existing CWTS data from the server, if we don't have it locally.
                    manager.start(SyncReason.FirstSync)
                    result.complete(Unit)
                }
            } else {
                result.complete(Unit)
            }
        }

        return result
    }

    /**
     * @return A list of currently supported [SyncEngine]s. `null` if sync isn't configured.
     */
    fun supportedSyncEngines(): Set<SyncEngine>? {
        // Notes on why this exists:
        // Parts of the system that make up an "fxa + sync" experience need to know which engines
        // are supported by an application. For example, FxA web content UI may present a "choose what
        // to sync" dialog during account sign-up, and application needs to be able to configure that
        // dialog. A list of supported engines comes to us from the application via passed-in SyncConfig.
        // Naturally, we could let the application configure any other part of the system that needs
        // to have access to supported engines. From the implementor's point of view, this is an extra
        // hurdle - instead of configuring only the account manager, they need to configure additional
        // classes. Additionally, we currently allow updating sync config "in-flight", not just at
        // the time of initialization. Providing an API for accessing currently configured engines
        // makes re-configuring SyncConfig less error-prone, as only one class needs to be told of the
        // new config.
        // Merits of allowing applications to re-configure SyncConfig after initialization are under
        // question, however: currently, we do not use that capability.
        return syncConfig?.supportedEngines
    }

    /**
     * Request an immediate synchronization, as configured according to [syncConfig].
     *
     * @param reason A [SyncReason] indicating why this sync is being requested.
     * @param debounce Boolean flag indicating if this sync may be debounced (in case another sync executed recently).
     */
    fun syncNowAsync(
        reason: SyncReason,
        debounce: Boolean = false
    ): Deferred<Unit> = CoroutineScope(coroutineContext).async {
        // We may be in a state where we're not quite authenticated yet - but can auto-retry our
        // authentication, instead.
        // This is one of our trigger points for retrying - when a user asks us to sync.
        // Another trigger point is the initialization flow of the account manager itself.
        if (accountMigrationInFlight()) {
            // Only trigger a retry attempt if the sync request was caused by a direct user action.
            // We don't attempt to retry migration on 'SyncReason.Startup' because a retry will happen during
            // account manager initialization if necessary anyway.
            // If the retry attempt succeeds, sync will run as a by-product of regular account authentication flow,
            // which is why we don't check for the result of a retry attempt here.
            when (reason) {
                SyncReason.User, SyncReason.EngineChange -> processQueueAsync(Event.RetryMigration).await()
            }
        } else {
            // Make sure auth cache is populated before we try to sync.
            maybeUpdateSyncAuthInfoCache()

            // Access to syncManager is guarded by `this`.
            synchronized(this) {
                if (syncManager == null) {
                    throw IllegalStateException(
                        "Sync is not configured. Construct this class with a 'syncConfig' or use 'setSyncConfig'"
                    )
                }
                syncManager?.now(reason, debounce)
            }
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
    fun authenticatedAccount(): OAuthAccount? = when (state) {
        AccountState.AuthenticatedWithProfile,
        AccountState.AuthenticatedNoProfile,
        AccountState.AuthenticationProblem,
        AccountState.CanAutoRetryAuthenticationViaTokenReuse,
        AccountState.CanAutoRetryAuthenticationViaTokenCopy -> account
        else -> null
    }

    /**
     * Checks if there's an in-flight account migration. An in-flight migration means that we've tried to "migrate"
     * via [signInWithShareableAccountAsync] and failed for intermittent (e.g. network) reasons.
     * A migration sign-in attempt will be retried automatically either during account manager initialization,
     * or as a by-product of user-triggered [syncNowAsync].
     */
    fun accountMigrationInFlight(): Boolean = when (state) {
        AccountState.CanAutoRetryAuthenticationViaTokenReuse,
        AccountState.CanAutoRetryAuthenticationViaTokenCopy -> true
        else -> false
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

    /**
     * Finalize authentication that was started via [beginAuthenticationAsync].
     *
     * If authentication wasn't started via this manager we won't accept this authentication attempt,
     * returning `false`. This may happen if [WebChannelFeature] is enabled, and user is manually
     * logging into accounts.firefox.com in a regular tab.
     *
     * Guiding principle behind this is that logging into accounts.firefox.com should not affect
     * logged-in state of the browser itself, even though the two may have an established communication
     * channel via [WebChannelFeature].
     *
     * @return A deferred boolean flag indicating if authentication state was accepted.
     */
    fun finishAuthenticationAsync(authData: FxaAuthData): Deferred<Boolean> {
        val result = CompletableDeferred<Boolean>()

        when {
            latestAuthState == null -> {
                logger.warn("Trying to finish authentication that was never started.")
                result.complete(false)
            }
            authData.state != latestAuthState -> {
                logger.warn("Trying to finish authentication for an invalid auth state; ignoring.")
                result.complete(false)
            }
            authData.state == latestAuthState -> {
                CoroutineScope(coroutineContext).launch {
                    processQueueAsync(Event.Authenticated(authData)).await()
                    result.complete(true)
                }
            }
            else -> throw IllegalStateException("Unexpected finishAuthenticationAsync state")
        }

        return result
    }

    fun logoutAsync(): Deferred<Unit> {
        return processQueueAsync(Event.Logout)
    }

    fun registerForAccountEvents(observer: AccountEventsObserver, owner: LifecycleOwner, autoPause: Boolean) {
        accountEventObserverRegistry.register(observer, owner, autoPause)
    }

    fun registerForSyncEvents(observer: SyncStatusObserver, owner: LifecycleOwner, autoPause: Boolean) {
        syncStatusObserverRegistry.register(observer, owner, autoPause)
    }

    override fun close() {
        GlobalAccountManager.close()
        coroutineContext.cancel()
        account.close()
    }

    internal suspend fun encounteredAuthError(
        operation: String,
        errorCountWithinTheTimeWindow: Int = 1
    ) = withContext(coroutineContext) {
        processQueueAsync(
            Event.AuthenticationError(operation, errorCountWithinTheTimeWindow)
        ).await()
    }

    /**
     * Pumps the state machine until all events are processed and their side-effects resolve.
     */
    private fun processQueueAsync(event: Event): Deferred<Unit> = CoroutineScope(coroutineContext).async {
        eventQueue.add(event)
        do {
            val toProcess: Event = eventQueue.poll()!!
            val transitionInto = FxaStateMatrix.nextState(state, toProcess)

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
    @Suppress("ComplexMethod", "ReturnCount", "ThrowsCount", "NestedBlockDepth", "LongMethod")
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
                        account = try {
                            getAccountStorage().read()
                        } catch (e: FxaPanicException) {
                            // Don't swallow panics from the underlying library.
                            throw e
                        } catch (e: FxaException) {
                            logger.error("Failed to load saved account. Re-initializing...", e)
                            null
                        } ?: return Event.AccountNotFound

                        // We may have attempted a migration previously, which failed in a way that allows
                        // us to retry it (e.g. a migration could have hit
                        when (account.isInMigrationState()) {
                            InFlightMigrationState.NONE -> Event.AccountRestored
                            InFlightMigrationState.REUSE_SESSION_TOKEN -> Event.InFlightReuseMigration
                            InFlightMigrationState.COPY_SESSION_TOKEN -> Event.InFlightCopyMigration
                        }
                    }
                    else -> null
                }
            }
            AccountState.NotAuthenticated -> {
                when (via) {
                    Event.FailedToAuthenticate, Event.Logout -> {
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
                        // Even though we might not have Sync enabled, clear out sync-related storage
                        // layers as well; if they're already empty (unused), nothing bad will happen
                        // and extra overhead is quite small.
                        SyncAuthInfoCache(context).clear()
                        SyncEnginesStorage(context).clear()
                        clearSyncState(context)
                        // Re-initialize account.
                        account = createAccount(serverConfig)

                        if (via is Event.Logout) {
                            notifyObservers { onLoggedOut() }
                        }

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

                        // "reusing an account" in this case means that we will maintain the same
                        // session token, enabling us to re-use the same FxA device record and, in
                        // theory, push subscriptions.
                        val migrationResult = if (via.reuseAccount) {
                            account.migrateFromSessionTokenAsync(
                                via.account.authInfo.sessionToken,
                                via.account.authInfo.kSync,
                                via.account.authInfo.kXCS
                            ).await()
                        } else {
                            account.copyFromSessionTokenAsync(
                                via.account.authInfo.sessionToken,
                                via.account.authInfo.kSync,
                                via.account.authInfo.kXCS
                            ).await()
                        }

                        // If a migration fails, we need to determine if it failed for a reason that allows
                        // retrying - e.g. an intermittent issue, where `isInMigrationState` will be 'true'.
                        // We model "ability to retry an in-flight migration" as a distinct state.
                        // This allows keeping this retry logic somewhat more contained, and not involved
                        // in a regular flow of things.

                        // So, below we will return the following:
                        // - `null` if migration failed in an unrecoverable way
                        // - SignedInShareableAccount if we succeeded
                        // - One of the 'retry' events if we can retry later

                        // Currently, we don't really care about the returned json blob. Right now all
                        // it contains is how long a migration took - which really is just measuring
                        // network performance for this particular operation.

                        if (migrationResult != null) {
                            return Event.SignedInShareableAccount(via.reuseAccount)
                        }

                        when (account.isInMigrationState()) {
                            InFlightMigrationState.NONE -> {
                                null
                            }
                            InFlightMigrationState.COPY_SESSION_TOKEN -> {
                                if (via.reuseAccount) {
                                    throw IllegalStateException("Expected 'reuse' in-flight state, saw 'copy'")
                                }
                                Event.RetryLaterViaTokenCopy
                            }
                            InFlightMigrationState.REUSE_SESSION_TOKEN -> {
                                if (!via.reuseAccount) {
                                    throw IllegalStateException("Expected 'copy' in-flight state, saw 'reuse'")
                                }
                                Event.RetryLaterViaTokenReuse
                            }
                        }
                    }
                    is Event.Pair -> {
                        val authFlowUrl = account.beginPairingFlowAsync(via.pairingUrl, scopes).await()
                        if (authFlowUrl == null) {
                            oauthObservers.notifyObservers { onError() }
                            return Event.FailedToAuthenticate
                        }
                        latestAuthState = authFlowUrl.state
                        oauthObservers.notifyObservers { onBeginOAuthFlow(authFlowUrl) }
                        null
                    }
                    else -> null
                }
            }
            AccountState.CanAutoRetryAuthenticationViaTokenReuse -> {
                when (via) {
                    Event.RetryMigration,
                    Event.InFlightReuseMigration -> {
                        logger.info("Registering persistence callback")
                        account.registerPersistenceCallback(statePersistenceCallback)
                        return retryMigration(reuseAccount = true)
                    }
                    else -> null
                }
            }
            AccountState.CanAutoRetryAuthenticationViaTokenCopy -> {
                when (via) {
                    Event.RetryMigration,
                    Event.InFlightCopyMigration -> {
                        logger.info("Registering persistence callback")
                        account.registerPersistenceCallback(statePersistenceCallback)
                        return retryMigration(reuseAccount = false)
                    }
                    else -> null
                }
            }
            AccountState.AuthenticatedNoProfile -> {
                when (via) {
                    is Event.Authenticated -> {
                        logger.info("Registering persistence callback")
                        account.registerPersistenceCallback(statePersistenceCallback)

                        // TODO do not ignore this failure.
                        logger.info("Completing oauth flow")

                        // Reasons this can fail:
                        // - network errors
                        // - unknown auth state
                        //  -- authenticating via web-content; we didn't beginOAuthFlowAsync
                        account.completeOAuthFlowAsync(via.authData.code, via.authData.state).await()

                        logger.info("Registering device constellation observer")
                        account.deviceConstellation().register(accountEventsIntegration)

                        logger.info("Initializing device")
                        // NB: underlying API is expected to 'ensureCapabilities' as part of device initialization.
                        if (account.deviceConstellation().initDeviceAsync(
                            deviceConfig.name, deviceConfig.type, deviceConfig.capabilities
                        ).await()) {
                            postAuthenticated(via.authData.authType, via.authData.declinedEngines)
                            Event.FetchProfile
                        } else {
                            Event.FailedToAuthenticate
                        }
                    }
                    Event.AccountRestored -> {
                        logger.info("Registering persistence callback")
                        account.registerPersistenceCallback(statePersistenceCallback)

                        logger.info("Registering device constellation observer")
                        account.deviceConstellation().register(accountEventsIntegration)

                        // If this is the first time ensuring our capabilities,
                        logger.info("Ensuring device capabilities...")
                        if (account.deviceConstellation().ensureCapabilitiesAsync(deviceConfig.capabilities).await()) {
                            logger.info("Successfully ensured device capabilities. Continuing...")
                            postAuthenticated(AuthType.Existing)
                            Event.FetchProfile
                        } else {
                            logger.warn("Failed to ensure device capabilities. Stopping.")
                            null
                        }
                    }
                    is Event.SignedInShareableAccount -> {
                        // Note that we are not registering an account persistence callback here like
                        // we do in other `AuthenticatedNoProfile` methods, because it would have been
                        // already registered while handling any of the pre-cursor events, such as
                        // `Event.SignInShareableAccount`, `Event.InFlightCopyMigration`
                        // or `Event.InFlightReuseMigration`.
                        logger.info("Registering device constellation observer")
                        account.deviceConstellation().register(accountEventsIntegration)

                        val deviceFinalizeSuccess = if (via.reuseAccount) {
                            logger.info("Configuring migrated account's device")
                            // At the minimum, we need to "ensure capabilities" - that is, register for Send Tab, etc.
                            account.deviceConstellation().ensureCapabilitiesAsync(deviceConfig.capabilities).await()
                            // Note that at this point, we do not rename a device record to our own default name.
                            // It's possible that user already customized their name, and we'd like to retain it.
                        } else {
                            logger.info("Initializing device")
                            // NB: underlying API is expected to 'ensureCapabilities' as part of device initialization.
                            account.deviceConstellation().initDeviceAsync(
                                deviceConfig.name, deviceConfig.type, deviceConfig.capabilities
                            ).await()
                        }

                        if (deviceFinalizeSuccess) {
                            postAuthenticated(AuthType.Shared)
                            Event.FetchProfile
                        } else {
                            Event.FailedToAuthenticate
                        }
                    }

                    Event.RecoveredFromAuthenticationProblem -> {
                        // This path is a blend of "authenticated" and "account restored".
                        // We need to re-initialize an fxa device, but we don't need to complete an auth.
                        logger.info("Registering persistence callback")
                        account.registerPersistenceCallback(statePersistenceCallback)

                        logger.info("Registering device constellation observer")
                        account.deviceConstellation().register(accountEventsIntegration)

                        // NB: we're running neither `initDevice` nor `ensureCapabilities` here, since
                        // this is a recovery flow, and these calls already ran at some point before.

                        postAuthenticated(AuthType.Recovered)

                        Event.FetchProfile
                    }
                    Event.FetchProfile -> {
                        // Profile fetching and account authentication issues:
                        // https://github.com/mozilla/application-services/issues/483
                        logger.info("Fetching profile...")

                        // `account` provides us with intelligent profile caching, so make sure to use it.
                        profile = account.getProfileAsync(ignoreCache = false).await()
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

                        // Ensure we clear any auth-relevant internal state, such as access tokens.
                        account.authErrorDetected()

                        // Clear our access token cache; it'll be re-populated as part of the
                        // regular state machine flow if we manage to recover.
                        SyncAuthInfoCache(context).clear()

                        // Circuit-breaker logic to protect ourselves from getting into endless authorization
                        // check loops. If we determine that application is trying to check auth status too
                        // frequently, drive the state machine into an unauthorized state.
                        if (via.errorCountWithinTheTimeWindow >= AUTH_CHECK_CIRCUIT_BREAKER_COUNT) {
                            crashReporter?.submitCaughtException(
                                AccountManagerException.AuthRecoveryCircuitBreakerException(via.operation)
                            )
                            logger.warn("Unable to recover from an auth problem, triggered a circuit breaker.")

                            notifyObservers { onAuthenticationProblems() }

                            return null
                        }

                        // Since we haven't hit the circuit-breaker above, perform an authorization check.
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
                                logger.info("Unable to recover from an auth problem, notifying observers.")

                                // Tell our listeners we're in a bad auth state.
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
        val authFlowUrl = account.beginOAuthFlowAsync(scopes).await()
        if (authFlowUrl == null) {
            oauthObservers.notifyObservers { onError() }
            return Event.FailedToAuthenticate
        }
        latestAuthState = authFlowUrl.state
        oauthObservers.notifyObservers { onBeginOAuthFlow(authFlowUrl) }
        return null
    }

    private suspend fun postAuthenticated(authType: AuthType, declinedEngines: Set<SyncEngine>? = null) {
        // Sync may not be configured at all (e.g. won't run), but if we received a
        // list of declined engines, that indicates user intent, so we preserve it
        // within SyncEnginesStorage regardless. If sync becomes enabled later on,
        // we will be respecting user choice around what to sync.
        declinedEngines?.let {
            val enginesStorage = SyncEnginesStorage(context)
            declinedEngines.forEach { declinedEngine ->
                enginesStorage.setStatus(declinedEngine, false)
            }

            // Enable all engines that were not explicitly disabled. Only do this in
            // the presence of a "declinedEngines" list, since that indicates user
            // intent. Absence of that list means that user was not presented with a
            // choice during authentication, and so we can't assume an enabled state
            // for any of the engines.
            syncConfig?.supportedEngines?.forEach { supportedEngine ->
                if (!declinedEngines.contains(supportedEngine)) {
                    enginesStorage.setStatus(supportedEngine, true)
                }
            }
        }

        // Before any sync workers have a chance to access it, make sure our SyncAuthInfo cache is hot.
        maybeUpdateSyncAuthInfoCache()

        // Notify our internal (sync) and external (app logic) observers.
        notifyObservers { onAuthenticated(account, authType) }

        FxaDeviceSettingsCache(context).setToCache(DeviceSettings(
            fxaDeviceId = account.getCurrentDeviceId()!!,
            name = deviceConfig.name,
            type = deviceConfig.type.intoSyncType()
        ))

        // If device supports SEND_TAB, and we're not recovering from an auth problem...
        if (deviceConfig.capabilities.contains(DeviceCapability.SEND_TAB) && authType != AuthType.Recovered) {
            // ... update constellation state
            account.deviceConstellation().refreshDevicesAsync().await()
        }
    }

    private suspend fun retryMigration(reuseAccount: Boolean): Event {
        val resultJson = account.retryMigrateFromSessionTokenAsync().await()
        if (resultJson != null) {
            return Event.SignedInShareableAccount(reuseAccount)
        }

        return when (account.isInMigrationState()) {
            InFlightMigrationState.NONE -> Event.FailedToAuthenticate
            InFlightMigrationState.COPY_SESSION_TOKEN -> {
                if (reuseAccount) {
                    throw IllegalStateException("Expected 'reuse' in-flight state, saw 'copy'")
                }
                Event.RetryLaterViaTokenCopy
            }
            InFlightMigrationState.REUSE_SESSION_TOKEN -> {
                if (!reuseAccount) {
                    throw IllegalStateException("Expected 'copy' in-flight state, saw 'reuse'")
                }
                Event.RetryLaterViaTokenReuse
            }
        }
    }

    @VisibleForTesting
    open fun createAccount(config: ServerConfig): OAuthAccount {
        return FirefoxAccount(config, null, crashReporter)
    }

    @VisibleForTesting
    internal open fun createSyncManager(config: SyncConfig): SyncManager {
        return WorkManagerSyncManager(context, config)
    }

    @VisibleForTesting
    internal open fun getAccountStorage(): AccountStorage {
        return if (deviceConfig.secureStateAtRest) {
            SecureAbove22AccountStorage(context, crashReporter)
        } else {
            SharedPrefAccountStorage(context, crashReporter)
        }
    }

    /**
     * In the future, this could be an internal account-related events processing layer.
     * E.g., once we grow events such as "please logout".
     * For now, we just pass everything downstream as-is.
     */
    private class AccountEventsIntegration(
        private val listenerRegistry: ObserverRegistry<AccountEventsObserver>
    ) : AccountEventsObserver {
        private val logger = Logger("AccountEventsIntegration")

        override fun onEvents(events: List<AccountEvent>) {
            logger.info("Received events, notifying listeners")
            listenerRegistry.notifyObservers { onEvents(events) }
        }
    }

    /**
     * Account status events flowing into the sync manager.
     */
    @VisibleForTesting
    internal class AccountsToSyncIntegration(
        private val syncManager: SyncManager
    ) : AccountObserver {
        override fun onLoggedOut() {
            syncManager.stop()
        }

        override fun onAuthenticated(account: OAuthAccount, authType: AuthType) {
            val reason = when (authType) {
                is AuthType.OtherExternal, AuthType.Signin, AuthType.Signup, AuthType.Shared, AuthType.Pairing
                    -> SyncReason.FirstSync
                AuthType.Existing, AuthType.Recovered -> SyncReason.Startup
            }
            syncManager.start(reason)
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
