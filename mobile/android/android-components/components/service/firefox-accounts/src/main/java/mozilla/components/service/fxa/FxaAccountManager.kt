/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import android.content.Context
import android.support.annotation.VisibleForTesting
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.async
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import mozilla.components.concept.sync.SyncDispatcher
import mozilla.components.concept.sync.SyncManager
import java.util.concurrent.ConcurrentLinkedQueue
import kotlin.coroutines.CoroutineContext

import mozilla.components.support.base.log.Log
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import java.io.Closeable
import java.util.concurrent.Executors

enum class AccountState {
    Start,
    NotAuthenticated,
    AuthenticatedNoProfile,
    AuthenticatedWithProfile
}

/**
 * Base class for account manager state machine events.
 * Events aren't a simple enum class because we might want to pass data along with some of the events.
 */
internal sealed class Event {
    override fun toString(): String {
        // For a better logcat experience.
        return this.javaClass.simpleName
    }

    internal object Init : Event()

    object AccountNotFound : Event()
    object AccountRestored : Event()

    object Authenticate : Event()
    data class Authenticated(val code: String, val state: String) : Event() {
        override fun toString(): String {
            // data classes define their own toString, so we override it here as well as in the base
            // class to avoid exposing 'code' and 'state' in logs.
            return this.javaClass.simpleName
        }
    }
    object FetchProfile : Event()
    object FetchedProfile : Event()

    object FailedToAuthenticate : Event()
    object FailedToFetchProfile : Event()

    object Logout : Event()
}

/**
 * Propagated via [AccountObserver.onError] if we fail to load a locally stored account during
 * initialization. No action is necessary from consumers.
 * Account state has been re-initialized.
 *
 * @param cause Optional original cause of failure.
 */
class FailedToLoadAccountException(cause: Exception?) : Exception(cause)

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
     * Account manager encountered an error during authentication. Inspect [error] for details.
     * @param error A specific FxA error encountered.
     */
    fun onError(error: FxaException)
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
 * @param config A [Config] used for account initialization.
 * @param scopes A list of scopes which will be requested during account authentication.
 */
@Suppress("TooManyFunctions")
open class FxaAccountManager(
    private val context: Context,
    private val config: Config,
    private val scopes: Array<String>,
    syncManager: SyncManager? = null,
    private val accountStorage: AccountStorage = SharedPrefAccountStorage(context)
) : Closeable, Observable<AccountObserver> by ObserverRegistry() {
    private val logTag = "FirefoxAccountStateMachine"

    private val oauthObservers = object : Observable<OAuthObserver> by ObserverRegistry() {}

    init {
        syncManager?.let { this.register(SyncManagerIntegration(it)) }
    }

    companion object {
        /**
         * State transition matrix. It's in the companion object to enforce purity.
         * @return An optional [AccountState] if provided state+event combination results in a
         * state transition. Note that states may transition into themselves.
         */
        @Suppress("ComplexMethod")
        internal fun nextState(state: AccountState, event: Event): AccountState? {
            return when (state) {
                AccountState.Start -> {
                    when (event) {
                        Event.Init -> AccountState.Start
                        Event.AccountNotFound -> AccountState.NotAuthenticated
                        Event.AccountRestored -> AccountState.AuthenticatedNoProfile
                        else -> null
                    }
                }
                AccountState.NotAuthenticated -> {
                    when (event) {
                        Event.Authenticate -> AccountState.NotAuthenticated
                        Event.FailedToAuthenticate -> AccountState.NotAuthenticated
                        is Event.Authenticated -> AccountState.AuthenticatedNoProfile
                        else -> null
                    }
                }
                AccountState.AuthenticatedNoProfile -> {
                    when (event) {
                        Event.FetchProfile -> AccountState.AuthenticatedNoProfile
                        Event.FetchedProfile -> AccountState.AuthenticatedWithProfile
                        Event.FailedToFetchProfile -> AccountState.AuthenticatedNoProfile
                        Event.Logout -> AccountState.NotAuthenticated
                        else -> null
                    }
                }
                AccountState.AuthenticatedWithProfile -> {
                    when (event) {
                        Event.Logout -> AccountState.NotAuthenticated
                        else -> null
                    }
                }
            }
        }
    }

    private val job = SupervisorJob()
    // We want a single-threaded execution model for our account-related "actions" (state machine side-effects).
    // That is, we want to ensure a sequential execution flow, but on a background thread.
    private val coroutineContext: CoroutineContext
        get() = Executors.newSingleThreadExecutor().asCoroutineDispatcher() + job

    // 'account' is initialized during processing of an 'Init' event.
    // Note on threading: we use a single-threaded executor, so there's no concurrent access possible.
    // However, that executor doesn't guarantee that it'll always use the same thread, and so vars
    // are marked as volatile for across-thread visibility. Similarly, event queue uses a concurrent
    // list, although that's probably an overkill.
    @Volatile private lateinit var account: OAuthAccount
    @Volatile private var profile: Profile? = null
    @Volatile private var state = AccountState.Start
    private val eventQueue = ConcurrentLinkedQueue<Event>()

    /**
     * Call this after registering your observers, and before interacting with this class.
     */
    fun initAsync(): Deferred<Unit> {
        return processQueueAsync(Event.Init)
    }

    fun authenticatedAccount(): OAuthAccount? {
        return when (state) {
            AccountState.AuthenticatedWithProfile,
            AccountState.AuthenticatedNoProfile -> account
            else -> null
        }
    }

    fun accountProfile(): Profile? {
        return when (state) {
            AccountState.AuthenticatedWithProfile -> profile
            else -> null
        }
    }

    fun updateProfileAsync(): Deferred<Unit> {
        return processQueueAsync(Event.FetchProfile)
    }

    fun beginAuthenticationAsync(): Deferred<String> {
        val deferredAuthUrl: CompletableDeferred<String> = CompletableDeferred()

        oauthObservers.register(object : OAuthObserver {
            override fun onBeginOAuthFlow(authUrl: String) {
                oauthObservers.unregister(this)
                deferredAuthUrl.complete(authUrl)
            }

            override fun onError(error: FxaException) {
                oauthObservers.unregister(this)
                deferredAuthUrl.completeExceptionally(error)
            }
        })

        processQueueAsync(Event.Authenticate)

        return deferredAuthUrl
    }

    fun finishAuthenticationAsync(code: String, state: String): Deferred<Unit> {
        return processQueueAsync(Event.Authenticated(code, state))
    }

    fun logoutAsync(): Deferred<Unit> {
        return processQueueAsync(Event.Logout)
    }

    override fun close() {
        job.cancel()
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
                Log.log(
                    tag = logTag,
                    message = "Got invalid event $toProcess for state $state."
                )
                continue
            }

            Log.log(
                tag = logTag,
                message = "Processing event $toProcess for state $state. Next state is $transitionInto"
            )

            state = transitionInto

            stateActions(state, toProcess)?.let { successiveEvent ->
                Log.log(
                    tag = logTag,
                    message = "Ran '$toProcess' side-effects for state $state, got successive event $successiveEvent"
                )
                eventQueue.add(successiveEvent)
            }
        } while (!eventQueue.isEmpty())
    }

    /**
     * Side-effects matrix. Defines non-pure operations that must take place for state+event combinations.
     */
    @Suppress("ComplexMethod")
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
                            accountStorage.read()
                        } catch (e: FxaException) {
                            Log.log(
                                tag = logTag,
                                priority = Log.Priority.ERROR,
                                throwable = e,
                                message = "Failed to load saved account."
                            )

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
                        // Clean up resources.
                        profile = null
                        account.close()
                        // Delete persisted state.
                        accountStorage.clear()
                        // Re-initialize account.
                        account = createAccount(config)

                        notifyObservers { onLoggedOut() }

                        null
                    }
                    Event.AccountNotFound -> {
                        account = createAccount(config)

                        notifyObservers { onLoggedOut() }

                        null
                    }
                    Event.Authenticate -> {
                        val url = try {
                            account.beginOAuthFlow(scopes, true).await()
                        } catch (e: FxaException) {
                            oauthObservers.notifyObservers { onError(e) }
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
                        account.completeOAuthFlow(via.code, via.state).await()
                        accountStorage.write(account)

                        notifyObservers { onAuthenticated(account) }

                        Event.FetchProfile
                    }
                    Event.AccountRestored -> {
                        notifyObservers { onAuthenticated(account) }

                        Event.FetchProfile
                    }
                    Event.FetchProfile -> {
                        // Profile fetching and account authentication issues:
                        // https://github.com/mozilla/application-services/issues/483
                        Log.log(tag = logTag, message = "Fetching profile...")
                        profile = try {
                            account.getProfile(true).await()
                        } catch (e: FxaException) {
                            Log.log(
                                Log.Priority.ERROR,
                                message = "Failed to get profile for authenticated account",
                                throwable = e,
                                tag = logTag
                            )

                            notifyObservers { onError(e) }

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
        }
    }

    @VisibleForTesting
    open fun createAccount(config: Config): OAuthAccount {
        return FirefoxAccount(config)
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

        override fun onError(error: Exception) {
            // TODO deal with FxaUnauthorizedException this at the state machine level.
            // This exception should cause a "logged out" transition.
        }
    }
}
