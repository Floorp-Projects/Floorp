/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.dataprotect

import android.content.Context
import android.content.SharedPreferences
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.collect
import java.lang.Exception

/**
 * This class exists so that we can measure how reliable our usage of AndroidKeyStore is.
 *
 * All of the actions here are executed against SecureAbove22Preferences, which encrypts/decrypts prefs
 * using a key managed by AndroidKeyStore.
 * If device is running on API<23, encryption/decryption won't be used.
 *
 * Experiment actions are:
 * - on every invocation, read a persisted value and verify it's correct; if it's missing write it.
 * - if an error is encountered (e.g. corrupt/missing value), experiment state is reset and the
 * experiment starts from scratch.
 *
 * For each step (get, write, reset), a Fact is emitted describing what happened (success, type of failure).
 * A special "experiment" Fact will be emitted in case of an unexpected failure.
 *
 * Consumers of this experiment are expected to inspect emitted Facts (e.g. record them into telemetry).
 */
class SecurePrefsReliabilityExperiment(private val context: Context) {
    companion object {
        const val PREFS_NAME = "KsReliabilityExp"
        const val PREF_DID_STORE_VALUE = "valueStored"
        const val SECURE_PREFS_NAME = "KsReliabilityExpSecure"
        const val PREF_KEY = "expKey"
        const val PREF_VALUE = "some long, mildly interesting string we'd like to store"

        object Actions {
            const val EXPERIMENT = "experiment"
            const val GET = "get"
            const val WRITE = "write"
            const val RESET = "reset"
        }

        @Suppress("MagicNumber")
        enum class Values(val v: Int) {
            SUCCESS_MISSING(1),
            SUCCESS_PRESENT(2),
            FAIL(3),
            LOST(4),
            CORRUPTED(5),
            PRESENT_UNEXPECTED(6),
            SUCCESS_WRITE(7),
            SUCCESS_RESET(8),
        }
    }

    private val securePrefs by lazy { SecureAbove22Preferences(context, SECURE_PREFS_NAME) }

    private fun prefs(): SharedPreferences {
        return context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
    }

    /**
     * Runs an experiment. This will emit one or more [Fact]s describing results.
     */
    @Suppress("TooGenericExceptionCaught", "ComplexMethod")
    operator fun invoke() {
        try {
            val storedVal = try {
                securePrefs.getString(PREF_KEY)
            } catch (e: Exception) {
                emitFact(Actions.GET, Values.FAIL, mapOf("javaClass" to e.nameForTelemetry()))

                // should this return? or proceed to the write part..?
                return
            }

            val valueAlreadyPersisted = prefs().getBoolean(PREF_DID_STORE_VALUE, false)

            val getResult = when {
                // we didn't store the value yet, and didn't get anything back either
                (!valueAlreadyPersisted && storedVal == null) -> {
                    Values.SUCCESS_MISSING
                }
                // we got back the value we stored
                (valueAlreadyPersisted && storedVal == PREF_VALUE) -> {
                    Values.SUCCESS_PRESENT
                }
                // value was lost
                (valueAlreadyPersisted && storedVal == null) -> {
                    Values.LOST
                }
                // we got some value back, but not what we stored
                (valueAlreadyPersisted && storedVal != PREF_VALUE) -> {
                    Values.CORRUPTED
                }
                // we didn't store the value yet, but got something back either way
                else -> {
                    Values.PRESENT_UNEXPECTED
                }
            }

            emitFact(Actions.GET, getResult)

            when (getResult) {
                // perform a write of the missing value
                Values.SUCCESS_MISSING -> {
                    try {
                        securePrefs.putString(PREF_KEY, PREF_VALUE)
                        emitFact(Actions.WRITE, Values.SUCCESS_WRITE)
                    } catch (e: Exception) {
                        emitFact(Actions.WRITE, Values.FAIL, mapOf("javaClass" to e.nameForTelemetry()))
                    }
                    prefs().edit().putBoolean(PREF_DID_STORE_VALUE, true).apply()
                }
                // reset our experiment in case of detected failures. this lets us measure the failure rate
                Values.LOST, Values.CORRUPTED, Values.PRESENT_UNEXPECTED -> {
                    securePrefs.clear()
                    prefs().edit().clear().apply()
                    emitFact(Actions.RESET, Values.SUCCESS_RESET)
                }
                else -> {
                    // no-op
                }
            }
        } catch (e: Exception) {
            emitFact(Actions.EXPERIMENT, Values.FAIL, mapOf("javaClass" to e.nameForTelemetry()))
        }
    }
}

private fun emitFact(
    item: String,
    value: SecurePrefsReliabilityExperiment.Companion.Values,
    metadata: Map<String, Any>? = null,
) {
    Fact(
        Component.LIB_DATAPROTECT,
        Action.IMPLEMENTATION_DETAIL,
        item,
        "${value.v}",
        metadata,
    ).collect()
}

private fun Exception.nameForTelemetry(): String {
    return this.javaClass.canonicalName ?: "anonymous"
}
