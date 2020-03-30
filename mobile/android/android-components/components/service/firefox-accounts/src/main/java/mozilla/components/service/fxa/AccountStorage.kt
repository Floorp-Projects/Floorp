/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import android.content.Context
import android.content.SharedPreferences
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.lib.dataprotect.SecureAbove22Preferences
import mozilla.components.support.base.crash.CrashReporting
import mozilla.components.support.base.log.logger.Logger

const val FXA_STATE_PREFS_KEY = "fxaAppState"
const val FXA_STATE_KEY = "fxaState"

internal interface AccountStorage {
    @Throws(Exception::class)
    fun read(): OAuthAccount?
    fun write(accountState: String)
    fun clear()
}

/**
 * Account storage layer which uses plaintext storage implementation.
 *
 * Migration from [SecureAbove22AccountStorage] will happen upon initialization,
 * unless disabled via [migrateFromSecureStorage].
 */
@SuppressWarnings("TooGenericExceptionCaught")
internal class SharedPrefAccountStorage(
    val context: Context,
    crashReporter: CrashReporting? = null,
    migrateFromSecureStorage: Boolean = true
) : AccountStorage {
    internal val logger = Logger("mozac/SharedPrefAccountStorage")

    init {
        if (migrateFromSecureStorage) {
            // In case we switched from SecureAbove22AccountStorage to this implementation, migrate persisted account
            // and clear out the old storage layer.
            val secureStorage = SecureAbove22AccountStorage(
                context,
                crashReporter,
                migrateFromPlaintextStorage = false
            )
            try {
                secureStorage.read()?.let { secureAccount ->
                    this.write(secureAccount.toJSONString())
                    secureStorage.clear()
                }
            } catch (e: Exception) {
                // Certain devices crash on various Keystore exceptions. While trying to migrate
                // to use the plaintext storage we don't want to crash if we can't access secure
                // storage, and just catch the errors.
                logger.error("Migrating from secure storage failed", e)
            }
        }
    }

    /**
     * @throws FxaException if JSON failed to parse into a [FirefoxAccount].
     */
    @Throws(FxaException::class)
    override fun read(): OAuthAccount? {
        val savedJSON = accountPreferences().getString(FXA_STATE_KEY, null)
                ?: return null

        // May throw a generic FxaException if it fails to process saved JSON.
        return FirefoxAccount.fromJSONString(savedJSON)
    }

    override fun write(accountState: String) {
        accountPreferences()
            .edit()
            .putString(FXA_STATE_KEY, accountState)
            .apply()
    }

    override fun clear() {
        accountPreferences()
            .edit()
            .remove(FXA_STATE_KEY)
            .apply()
    }

    private fun accountPreferences(): SharedPreferences {
        return context.getSharedPreferences(FXA_STATE_PREFS_KEY, Context.MODE_PRIVATE)
    }
}

/**
 * A base class for exceptions describing abnormal account storage behaviour.
 */
internal abstract class AbnormalAccountStorageEvent : Exception() {
    /**
     * Account state was expected to be present, but it wasn't.
     */
    internal class UnexpectedlyMissingAccountState : AbnormalAccountStorageEvent()
}

/**
 * Account storage layer which uses encrypted-at-rest storage implementation for supported API levels (23+).
 * On older API versions account state is stored in plaintext.
 *
 * Migration from [SharedPrefAccountStorage] will happen upon initialization,
 * unless disabled via [migrateFromPlaintextStorage].
 */
internal class SecureAbove22AccountStorage(
    context: Context,
    private val crashReporter: CrashReporting? = null,
    migrateFromPlaintextStorage: Boolean = true
) : AccountStorage {
    companion object {
        private const val STORAGE_NAME = "fxaStateAC"
        private const val KEY_ACCOUNT_STATE = "fxaState"
        private const val PREF_NAME = "fxaStatePrefAC"
        private const val PREF_KEY_HAS_STATE = "fxaStatePresent"
    }

    private val store = SecureAbove22Preferences(context, STORAGE_NAME)
    // Prefs are used here to keep track of abnormal storage behaviour - namely, account state disappearing without
    // being cleared first through this class. Note that clearing application data will clear both 'store' and 'prefs'.
    private val prefs = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE)

    init {
        if (migrateFromPlaintextStorage) {
            // In case we switched from SharedPrefAccountStorage to this implementation, migrate persisted account
            // and clear out the old storage layer.
            val plaintextStorage = SharedPrefAccountStorage(context, migrateFromSecureStorage = false)
            plaintextStorage.read()?.let { plaintextAccount ->
                this.write(plaintextAccount.toJSONString())
                plaintextStorage.clear()
            }
        }
    }

    /**
     * @throws FxaException if JSON failed to parse into a [FirefoxAccount].
     */
    @Throws(FxaException::class)
    override fun read(): OAuthAccount? {
        return store.getString(KEY_ACCOUNT_STATE).also {
            // If account state is missing, but we expected it to be present, report an exception.
            if (it == null && prefs.getBoolean(PREF_KEY_HAS_STATE, false)) {
                crashReporter?.submitCaughtException(AbnormalAccountStorageEvent.UnexpectedlyMissingAccountState())
                // Clear prefs to make sure we only submit this exception once.
                prefs.edit().clear().apply()
            }
        }?.let { FirefoxAccount.fromJSONString(it) }
    }

    override fun write(accountState: String) {
        store.putString(KEY_ACCOUNT_STATE, accountState)
        prefs.edit().putBoolean(PREF_KEY_HAS_STATE, true).apply()
    }

    override fun clear() {
        store.clear()
        prefs.edit().clear().apply()
    }
}
