/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import android.content.Context
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.sharing.ShareableAccount
import mozilla.components.service.fxa.sharing.ShareableAuthInfo
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.migration.FxaMigrationResult.Failure
import mozilla.components.support.migration.FxaMigrationResult.Success.SignedInIntoAuthenticatedAccount
import org.json.JSONException
import org.json.JSONObject
import java.io.File

/**
 * A Fennec Firefox account.
 */
private data class FennecAccount(val email: String, val stateLabel: String, val authInfo: FennecAuthenticationInfo?)

/**
 * Encapsulates authentication info necessary to sign-in into a [FennecAccount].
 */
private data class FennecAuthenticationInfo(
    val sessionToken: String,
    val kXCS: String,
    val kSync: String
) {
    override fun toString(): String {
        return "Authenticated account"
    }
}

/**
 * Wraps [FxaMigrationResult] in an exception so that it can be returned via [Result.Failure].
 *
 * PII note - be careful to not log this exception, as it may contain personal information (wrapped in a JSONException).
 *
 * @property failure Wrapped [FxaMigrationResult] indicating exact failure reason.
 */
class FxaMigrationException(val failure: Failure) : Exception()

/**
 * Result of an FxA migration.
 *
 * Note that toString implementations are overridden to help avoid accidental logging of PII.
 */
sealed class FxaMigrationResult {
    /**
     * Success variants of an FxA migration.
     */
    sealed class Success : FxaMigrationResult() {
        /**
         * No Fennec account found.
         */
        object NoAccount : Success()

        /**
         * Encountered a Fennec auth state that can't be used to automatically log-in.
         */
        data class UnauthenticatedAccount(val email: String, val stateLabel: String) : Success() {
            override fun toString(): String {
                return "Unauthenticated account in state $stateLabel"
            }
        }

        /**
         * Successfully signed-in with an authenticated Fennec account.
         */
        data class SignedInIntoAuthenticatedAccount(val email: String, val stateLabel: String) : Success() {
            override fun toString(): String {
                return "Signed-in into an authenticated account"
            }
        }
    }

    /**
     * Failure variants of an FxA migration.
     */
    sealed class Failure : FxaMigrationResult() {
        /**
         * Failed to process Fennec's auth state due to an exception [e].
         */
        data class CorruptAccountState(val e: JSONException) : Failure() {
            override fun toString(): String {
                return "Corrupt account state, exception type: ${e::class}"
            }
        }

        /**
         * Encountered an unsupported version of Fennec's auth state.
         */
        data class UnsupportedVersions(
            val accountVersion: Int,
            val pickleVersion: Int,
            val stateVersion: Int? = null
        ) : Failure() {
            override fun toString(): String {
                return "Unsupported version(s): account=$accountVersion, pickle=$pickleVersion, state=$stateVersion"
            }
        }

        /**
         * Failed to sign in into an authenticated account. Currently, this could be either due to network failures,
         * invalid credentials, or server-side issues.
         */
        data class FailedToSignIntoAuthenticatedAccount(val email: String, val stateLabel: String) : Failure()
    }
}

/**
 * Knows how to process an authenticated account detected during a migration.
 */
private object AuthenticatedAccountProcessor {
    internal suspend fun signIn(
        context: Context,
        accountManager: FxaAccountManager,
        fennecAccount: FennecAccount
    ): Result<FxaMigrationResult> {
        require(fennecAccount.authInfo != null) { "authInfo must be present in order to sign-in a FennecAccount" }

        val fennecAuthInfo = ShareableAuthInfo(
            sessionToken = fennecAccount.authInfo.sessionToken,
            kSync = fennecAccount.authInfo.kSync,
            kXCS = fennecAccount.authInfo.kXCS
        )
        val shareableAccount = ShareableAccount(
            email = fennecAccount.email,
            sourcePackage = context.packageName,
            authInfo = fennecAuthInfo
        )

        if (!accountManager.signInWithShareableAccountAsync(shareableAccount).await()) {
            // What do we do now?
            // We detected an account in a good state, and failed to actually login with it.
            // This could indicate that credentials stored in Fennec are no longer valid, or that we had a
            // network issue during the sign-in.
            // Ideally, we're able to detect which one it is, and maybe retry in case of network issues.
            // `signInWithShareableAccountAsync` needs to be expanded to support better error reporting.
            return Result.Failure(
                FxaMigrationException(Failure.FailedToSignIntoAuthenticatedAccount(
                    email = fennecAccount.email,
                    stateLabel = fennecAccount.stateLabel
                ))
            )
        }
        return Result.Success(SignedInIntoAuthenticatedAccount(
            email = fennecAccount.email,
            stateLabel = fennecAccount.stateLabel
        ))
    }
}

internal object FennecFxaMigration {
    private val logger = Logger("FennecFxaMigration")

    /**
     * Performs a migration of Fennec's FxA state located in [fxaStateFile].
     */
    suspend fun migrate(
        fxaStateFile: File,
        context: Context,
        accountManager: FxaAccountManager
    ): Result<FxaMigrationResult> {
        logger.debug("Migrating Fennec account at ${fxaStateFile.absolutePath}")

        if (!fxaStateFile.exists()) {
            return Result.Success(FxaMigrationResult.Success.NoAccount)
        }

        return try {
            val fennecAccount = parseJSON(fxaStateFile.readText())

            when (fennecAccount.authInfo) {
                // Found an account, but it wasn't authenticated.
                null -> Result.Success(
                    FxaMigrationResult.Success.UnauthenticatedAccount(fennecAccount.email, fennecAccount.stateLabel)
                )
                // Found an authenticated account. Try to sign-in.
                else -> AuthenticatedAccountProcessor.signIn(context, accountManager, fennecAccount)
            }
        } catch (e: FxaMigrationException) {
            Result.Failure(e)
        }
    }

    @Suppress("MagicNumber", "ReturnCount", "ComplexMethod", "ThrowsCount")
    @Throws(FxaMigrationException::class)
    private fun parseJSON(rawJSON: String): FennecAccount {
        val json = try {
            JSONObject(rawJSON)
        } catch (e: JSONException) {
            logger.error("Corrupt FxA state: couldn't parse pickle file", e)
            throw FxaMigrationException(Failure.CorruptAccountState(e))
        }

        val (accountVersion, pickleVersion) = try {
            json.getInt("account_version") to json.getInt("pickle_version")
        } catch (e: JSONException) {
            logger.error("Corrupt FxA state: couldn't read versions", e)
            throw FxaMigrationException(Failure.CorruptAccountState(e))
        }

        if (accountVersion != 3) {
            // Account version changed from 2 to 3 in Firefox 29 (released spring of 2014).
            // This is too long ago for us to worry about migrating older versions.
            // https://bugzilla.mozilla.org/show_bug.cgi?id=962320
            throw FxaMigrationException(Failure.UnsupportedVersions(accountVersion, pickleVersion))
        }

        if (pickleVersion != 3) {
            // Pickle version last changed in Firefox 38 (released winter of 2015).
            // This is too long ago for us to worry about migrating older versions.
            // https://bugzilla.mozilla.org/show_bug.cgi?id=1123107
            throw FxaMigrationException(Failure.UnsupportedVersions(accountVersion, pickleVersion))
        }

        try {
            val bundle = json.getJSONObject("bundle")
            val state = JSONObject(bundle.getString("state"))

            // We support migrating versions 3 and 4 of the embedded "fxa state". In Fennec, this "state" represents
            // a serialization of its state machine state. So, processing this state is the crux of our migration.
            // Version 4 was introduced in Firefox 59 (released spring of 2018).
            // See https://bugzilla.mozilla.org/show_bug.cgi?id=1426305
            return when (val stateVersion = state.getInt("version")) {
                // See https://github.com/mozilla-mobile/android-components/issues/4887
                // 3 -> processStateV3(state)
                4 -> processStateV4(bundle, state)
                else -> throw FxaMigrationException(
                    Failure.UnsupportedVersions(accountVersion, pickleVersion, stateVersion)
                )
            }
        } catch (e: JSONException) {
            logger.error("Corrupt FxA state: couldn't process state")
            throw FxaMigrationException(Failure.CorruptAccountState(e))
        }
    }

    @Throws(JSONException::class)
    private fun processStateV4(bundle: JSONObject, state: JSONObject): FennecAccount {
        val email = state.getString("email")

        // A state label represents in which Fennec FxA state machine state we find ourselves.
        // "Cohabiting" and "Married" are the two states in which we have the derived keys, kXCS and kSync,
        // which are necessary to decrypt sync data.
        // "Married" state is the obvious terminal state of the state machine.
        // "Cohabiting" state, as it relates to ability to authenticate new clients,
        // is described in https://bugzilla.mozilla.org/show_bug.cgi?id=1568336.
        // In essence, for regularly used Fennec instances with sync running at least once every 24hrs, we'll
        // be in the "Married" state. Otherwise, it's likely we'll find ourselves in the "Cohabiting" state.
        // NB: in both states, it's possible that our credentials are invalid, e.g. password could have been
        // changed remotely since Fennec last synced.
        val stateLabel = bundle.getString("stateLabel")
        if (!listOf("Cohabiting", "Married").contains(stateLabel)) {
            return FennecAccount(email = email, stateLabel = stateLabel, authInfo = null)
        }

        val sessionToken = state.getString("sessionToken")
        val kXCS = state.getString("kXCS")
        val kSync = state.getString("kSync")

        return FennecAccount(
            email = email,
            stateLabel = stateLabel,
            authInfo = FennecAuthenticationInfo(sessionToken = sessionToken, kSync = kSync, kXCS = kXCS)
        )
    }

    // private fun processStateV3(state: JSONObject): FxaMigrationResult {
        // See https://github.com/mozilla-mobile/android-components/issues/4887 where this is tracked.

        // In v3 fxa state, instead of storing derived kSync and kXCS, we stored kB itself.
        // This method should:
        // - read the kB
        // - derive sync key
        // - compute client state (kXCS)
        // See similar conversion in Fennec: https://searchfox.org/mozilla-esr68/source/mobile/android/services/src/main/java/org/mozilla/gecko/fxa/login/StateFactory.java#168-195
        // The rest of the state schema is the same as v4.
        // logger.debug("Processing account state v3 $state")
    // }
}
