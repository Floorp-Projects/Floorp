/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.dataprotect

import android.annotation.TargetApi
import android.content.Context
import android.content.Context.MODE_PRIVATE
import android.os.Build
import android.os.Build.VERSION_CODES.M
import android.util.Base64
import mozilla.components.support.base.log.logger.Logger
import java.nio.charset.StandardCharsets
import java.security.GeneralSecurityException

private const val BASE_64_FLAGS = Base64.URL_SAFE or Base64.NO_PADDING

/**
 * A wrapper around [SharedPreferences] which encrypts contents on supported API versions (23+).
 * Otherwise, this simply delegates to [SharedPreferences].
 *
 * In rare circumstances (such as APK signing key rotation) a master key which protects this storage may be lost,
 * in which case previously stored values will be lost as well. Applications are encouraged to instrument such events.
 *
 * @param context A [Context], used for accessing [SharedPreferences].
 * @param keystore An optional [Keystore]: required when running on API23+, expected as `null` otherwise.
 */
class KeySharedPreferences(
    context: Context,
    private val keystore: Keystore? = null
) {

    companion object {
        const val KEY_PREFERENCES = "key_preferences"
    }

    init {
        if (Build.VERSION.SDK_INT < M) {
            require(keystore == null) {
                "Keystore isn't supported for pre-23 API versions"
            }
        } else {
            require(keystore != null) {
                "Keystore must be configured for 23+ API versions"
            }
        }
    }

    private val logger = Logger("KeySharedPreferences")
    private val prefs = context.getSharedPreferences(KEY_PREFERENCES, MODE_PRIVATE)

    /**
     * Retrieves a stored [key]. See [putString] for storing a [key].
     *
     * @param key A key name.
     * @return An optional [String] if [key] is present in the store.
     */
    fun getString(key: String): String? {
        return if (Build.VERSION.SDK_INT >= M) {
            getStringM(key)
        } else {
            prefs.getString(key, null)
        }
    }

    /**
     * Stores [value] under [key]. Retrieve it using [getString].
     *
     * @param key A key name.
     * @param value A value for [key].
     */
    fun putString(key: String, value: String) {
        if (Build.VERSION.SDK_INT >= M) {
            putStringM(key, value)
        } else {
            prefs.edit().putString(key, value).apply()
        }
    }

    /**
     * Removes key/value pair from storage for the provided [key].
     */
    fun remove(key: String) {
        prefs.edit().remove(key).apply()
    }

    @TargetApi(M)
    private fun getStringM(key: String): String? {
        // The fact that we're possibly generating a managed key here implies that this key could be lost after being
        // for some reason. One possible reason for a key to be lost is rotating signing keys for the APK.
        // Applications are encouraged to instrument such events.
        generateManagedKeyIfNecessary()

        return if (prefs.contains(key)) {
            val value = prefs.getString(key, "")
            val encrypted = Base64.decode(value, BASE_64_FLAGS)

            val plain = try {
                keystore!!.decryptBytes(encrypted)
            } catch (error: IllegalArgumentException) {
                logger.error("IllegalArgumentException exception: ", error)
                null
            } catch (error: GeneralSecurityException) {
                logger.error("Decrypt exception: ", error)
                null
            }

            plain.let {
                if (it != null) String(it, StandardCharsets.UTF_8) else null
            }
        } else {
            null
        }
    }

    @TargetApi(M)
    private fun putStringM(key: String, value: String) {
        generateManagedKeyIfNecessary()
        val editor = prefs.edit()

        val encrypted = keystore!!.encryptBytes(value.toByteArray(StandardCharsets.UTF_8))
        val data = Base64.encodeToString(encrypted,
            BASE_64_FLAGS
        )

        editor.putString(key, data).apply()
    }

    /**
     * Generates a "managed key" - a key used to encrypt data stored by this class. This key is "managed" by [Keystore],
     * which stores it in system's secure storage layer exposed via [AndroidKeyStore].
     */
    private fun generateManagedKeyIfNecessary() {
        if (!keystore!!.available()) {
            keystore.generateKey()
        }
    }
}
