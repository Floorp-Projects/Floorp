/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import android.os.StrictMode
import androidx.annotation.GuardedBy
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.ext.resetPoliciesAfter
import kotlin.coroutines.CoroutineContext

/**
 * Miscellaneous FxA-related abnormalities.
 */
@VisibleForTesting
internal abstract class AbnormalFxaEvent : Exception() {
    /**
     * Indicates an overlapping sign-out request.
     */
    class OverlappingFxaLogoutRequest : AbnormalFxaEvent()

    /**
     * Indicates an onLogout callback which was received without a preceding onAuthenticated callback.
     */
    class LogoutWithoutAuth : AbnormalFxaEvent()

    /**
     * Indicates an unexpected sign-out event. All events must be user-triggered; this exception is
     * logged when a sign-out event was detected without a corresponding user action.
     */
    class UnexpectedFxaLogout : AbnormalFxaEvent()

    /**
     * Indicates an account that's missing after startup, while it was expected to be present.
     */
    class MissingExpectedAccountAfterStartup : AbnormalFxaEvent()
}

/**
 * Observes account-related events, and reports any detected abnormalities via [crashReporter].
 *
 * See [AbnormalFxaEvent] for types of abnormal events this class detects.
 *
 * @param crashReporter An instance of [CrashReporter] used for reporting detected abnormalities.
 * @param coroutineContext A [CoroutineContext] used for executing async tasks. Defaults to [Dispatchers.IO].
 */
class AccountAbnormalities(
    context: Context,
    private val crashReporter: CrashReporter,
    private val coroutineContext: CoroutineContext = Dispatchers.IO
) : AccountObserver {
    companion object {
        private const val PREF_FXA_ABNORMALITIES = "fxa_abnormalities"
        private const val KEY_HAS_ACCOUNT = "has_account"
    }

    @GuardedBy("this")
    private var isLoggingOut = false

    @Volatile
    private var accountManagerConfigured = false

    @Volatile
    private var onAuthenticatedCalled = false

    private val logger = Logger("AccountAbnormalities")

    private val prefs = StrictMode.allowThreadDiskReads().resetPoliciesAfter {
        context.getSharedPreferences(PREF_FXA_ABNORMALITIES, Context.MODE_PRIVATE)
    }

    /**
     * Once [accountManager] is initialized, queries it to detect abnormal account states.
     * Call this right after running [FxaAccountManager.initAsync].
     *
     * @param accountManager An instance of [FxaAccountManager].
     * @param initResult A deferred result of initializing [accountManager].
     * @return A [Unit] deferred, resolved once [initResult] is resolved and state is processed for abnormalities.
     */
    fun accountManagerInitializedAsync(
        accountManager: FxaAccountManager,
        initResult: Deferred<Unit>
    ): Deferred<Unit> {
        accountManagerConfigured = true

        return CoroutineScope(coroutineContext).async {
            // Wait for the account manager to finish initializing. If it's queried before the
            // "init" deferred returns, we'll get inaccurate results.
            initResult.await()

            // Account manager finished initialization, we can now query it for the account state
            // and see if it doesn't match our expectations.
            // Behaviour considered abnormal:
            // - we had an account before, and it's no longer present during startup

            // We use a flag in prefs to keep track of the fact that we have an authenticated
            // account. This works because our account state is persisted in the application's
            // directory, same as SharedPreferences. If user clears application data, both the
            // fxa state and our flag will be removed.
            val hadAccountBefore = prefs.getBoolean(KEY_HAS_ACCOUNT, false)
            val hasAccountNow = accountManager.authenticatedAccount() != null
            if (hadAccountBefore && !hasAccountNow) {
                prefs.edit().putBoolean(KEY_HAS_ACCOUNT, false).apply()

                logger.warn("Missing expected account on startup")

                crashReporter.submitCaughtException(
                    AbnormalFxaEvent.MissingExpectedAccountAfterStartup()
                )
            }
        }
    }

    /**
     * Keeps track of user requests to logout.
     */
    fun userRequestedLogout() {
        check(accountManagerConfigured) {
            "userRequestedLogout before account manager was configured"
        }

        // Expecting to have seen an onAuthenticated callback before a logout can be triggered.
        if (!onAuthenticatedCalled) {
            crashReporter.submitCaughtException(AbnormalFxaEvent.LogoutWithoutAuth())
        }

        // If we're not already in the process of logging out, do nothing.
        synchronized(this) {
            if (!isLoggingOut) {
                isLoggingOut = true
                return
            }
        }

        logger.warn("Overlapping logout request")

        // Otherwise, this is an unexpected logout request - there shouldn't be a legitimate way for
        // the user to request multiple overlapping logouts. Log an exception.
        crashReporter.submitCaughtException(AbnormalFxaEvent.OverlappingFxaLogoutRequest())
    }

    override fun onAuthenticated(account: OAuthAccount, authType: AuthType) {
        check(accountManagerConfigured) { "onAuthenticated before account manager was configured" }

        onAuthenticatedCalled = true

        // We don't check if KEY_HAS_ACCOUNT was already true: we will see onAuthenticated on every
        // startup, so any combination of "new value" and "previous value" for this flag is normal.
        prefs.edit().putBoolean(KEY_HAS_ACCOUNT, true).apply()
    }

    override fun onLoggedOut() {
        check(accountManagerConfigured) { "onLoggedOut before account manager was configured" }

        onAuthenticatedCalled = false

        prefs.edit().putBoolean(KEY_HAS_ACCOUNT, false).apply()

        // If we're in the process of logging out (via userRequestedLogout), do nothing.
        synchronized(this) {
            if (isLoggingOut) {
                isLoggingOut = false
                return
            }
        }

        logger.warn("Unexpected sign-out")

        // Otherwise, this is an unexpected logout event - all logout events are expected to be
        // user-triggered. Log an exception.
        crashReporter.submitCaughtException(AbnormalFxaEvent.UnexpectedFxaLogout())
    }
}
