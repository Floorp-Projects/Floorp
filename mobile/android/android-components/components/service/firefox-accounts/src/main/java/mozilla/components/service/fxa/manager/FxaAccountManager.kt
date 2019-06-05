/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.manager

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.lifecycle.LifecycleOwner
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.async
import kotlinx.coroutines.cancel
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthException
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.DeviceEvent
import mozilla.components.concept.sync.DeviceEventsObserver
import mozilla.components.concept.sync.DeviceType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import mozilla.components.concept.sync.StatePersistenceCallback
import mozilla.components.concept.sync.SyncManager
import mozilla.components.service.fxa.AccountStorage
import mozilla.components.service.fxa.Config
import mozilla.components.service.fxa.FirefoxAccount
import mozilla.components.service.fxa.FxaException
import mozilla.components.service.fxa.FxaPanicException
import mozilla.components.service.fxa.SharedPrefAccountStorage
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import java.io.Closeable
import java.lang.ref.WeakReference
import java.util.concurrent.ConcurrentLinkedQueue
import java.util.concurrent.Executors
import kotlin.coroutines.CoroutineContext

/**
 * Propagated via [AccountObserver.onError] if we fail to load a locally stored account during
 * initialization. No action is necessary from consumers.
 * Account state has been re-initialized.
 *
 * @param cause Optional original cause of failure.
 */
class FailedToLoadAccountException(cause: Exception?) : Exception(cause)

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

const val PROFILE_SCOPE = "profile"

/**
 * Helper data class that wraps common device initialization parameters.
 */
data class DeviceTuple(val name: String, val type: DeviceType, val capabilities: List<DeviceCapability>)

/**
 * An account manager which encapsulates various internal details of an account lifecycle and provides
 * an observer interface along with a public API for interacting with an account.
 * The internal state machine abstracts over state space as exposed by the fxaclient library, not
 * the internal states experienced by lower-level representation of a Firefox Account; those are opaque to us.
 *
 * Class is 'open' to facilitate testing.
 *
 * @param context A [Context] instance that's used for internal messaging and interacting with local storage.
 * @param config A [Config] used for account initialization.
 * @param applicationScopes A list of scopes which will be requested during account authentication.
 * @param deviceTuple A description of the current device (name, type, capabilities).
 */
@Suppress("TooManyFunctions")
open class FxaAccountManager(
    private val context: Context,
    private val config: Config,
    private val applicationScopes: Array<String>,
    private val deviceTuple: DeviceTuple,
    syncManager: SyncManager? = null,
    // We want a single-threaded execution model for our account-related "actions" (state machine side-effects).
    // That is, we want to ensure a sequential execution flow, but on a background thread.
    private val coroutineContext: CoroutineContext = Executors
            .newSingleThreadExecutor().asCoroutineDispatcher() + SupervisorJob()
) : Closeable, Observable<AccountObserver> by ObserverRegistry() {
    private val logger = Logger("FirefoxAccountStateMachine")

    // We always obtain a "profile" scope, in addition to whatever scopes application requested.
    private val scopes: Set<String> = setOf(PROFILE_SCOPE).plus(applicationScopes)

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
        syncManager?.let { this.register(SyncManagerIntegration(it)) }
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
            val toProcess = eventQueue.poll()
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
                            logger.error("Failed to load saved account.", e)

                            notifyObservers { onError(FailedToLoadAccountException(e)) }

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
                        // Destroy the current device record.
                        account.deviceConstellation().destroyCurrentDeviceAsync().await()
                        // Clean up resources.
                        profile = null
                        account.close()
                        // Delete persisted state.
                        getAccountStorage().clear()
                        // Re-initialize account.
                        account = createAccount(config)

                        notifyObservers { onLoggedOut() }

                        null
                    }
                    Event.AccountNotFound -> {
                        account = createAccount(config)

                        null
                    }
                    Event.Authenticate -> {
                        return doAuthenticate()
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
                            deviceTuple.name, deviceTuple.type, deviceTuple.capabilities
                        ).await()

                        logger.info("Starting periodic refresh of the device constellation")
                        account.deviceConstellation().startPeriodicRefresh()

                        notifyObservers { onAuthenticated(account) }

                        Event.FetchProfile
                    }
                    Event.AccountRestored -> {
                        logger.info("Registering persistence callback")
                        account.registerPersistenceCallback(statePersistenceCallback)

                        logger.info("Registering device constellation observer")
                        account.deviceConstellation().register(deviceEventsIntegration)

                        // If this is the first time ensuring our capabilities,
                        logger.info("Ensuring device capabilities...")
                        if (account.deviceConstellation().ensureCapabilitiesAsync(deviceTuple.capabilities).await()) {
                            logger.info("Successfully ensured device capabilities.")
                        } else {
                            logger.warn("Failed to ensure device capabilities.")
                        }

                        logger.info("Starting periodic refresh of the device constellation")
                        account.deviceConstellation().startPeriodicRefresh()

                        notifyObservers { onAuthenticated(account) }

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
                                deviceTuple.name, deviceTuple.type, deviceTuple.capabilities
                        ).await()

                        logger.info("Starting periodic refresh of the device constellation")
                        account.deviceConstellation().startPeriodicRefresh()

                        notifyObservers { onAuthenticated(account) }

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

                        // We request an access token for a "profile" scope since that's the only
                        // scope we're guaranteed to have at this point. That is, we don't rely on
                        // passed-in application-specific scopes.
                        when (account.checkAuthorizationStatusAsync(PROFILE_SCOPE).await()) {
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
                                account = createAccount(config)

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

    private suspend fun doAuthenticate(): Event? {
        val url = account.beginOAuthFlowAsync(scopes, true).await()
        if (url == null) {
            oauthObservers.notifyObservers { onError() }
            return Event.FailedToAuthenticate
        }
        oauthObservers.notifyObservers { onBeginOAuthFlow(url) }
        return null
    }

    @VisibleForTesting
    open fun createAccount(config: Config): OAuthAccount {
        return FirefoxAccount(config)
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

    private class SyncManagerIntegration(private val syncManager: SyncManager) : AccountObserver {
        override fun onLoggedOut() {
            syncManager.loggedOut()
        }

        override fun onAuthenticated(account: OAuthAccount) {
            syncManager.authenticated(account)
        }

        override fun onProfileUpdated(profile: Profile) {
            // SyncManager doesn't care about the FxA profile.
            // In the future, we might kick-off an immediate sync here.
        }

        override fun onAuthenticationProblems() {
            syncManager.loggedOut()
        }

        override fun onError(error: Exception) {
            // TODO deal with FxaUnauthorizedException this at the state machine level.
            // This exception should cause a "logged out" transition.
        }
    }
}
