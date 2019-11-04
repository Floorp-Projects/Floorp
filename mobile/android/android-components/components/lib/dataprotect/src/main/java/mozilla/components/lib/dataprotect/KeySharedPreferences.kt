/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

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

open class KeySharedPreferences(
    context: Context,
    private val keystore: Keystore? = Keystore(BuildConfig.APPLICATION_ID)
) {

    companion object {
        const val KEY_PREFERENCES = "key_preferences"
    }

    private val logger = Logger("KeySharedPreferences")
    private val prefs = context.getSharedPreferences(KEY_PREFERENCES, MODE_PRIVATE)

    open fun getString(key: String): String? {
        return if (Build.VERSION.SDK_INT >= M) {
            getStringM(key)
        } else {
            prefs.getString(key, null)
        }
    }

    private fun getStringM(key: String): String? {
        verifyKey()

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

    open fun putString(key: String, value: String) {
        if (Build.VERSION.SDK_INT >= M) {
            putStringM(key, value)
        } else {
            prefs.edit().putString(key, value).apply()
        }
    }

    private fun putStringM(key: String, value: String) {
        verifyKey()
        val editor = prefs.edit()

        val encrypted = keystore!!.encryptBytes(value.toByteArray(StandardCharsets.UTF_8))
        val data = Base64.encodeToString(encrypted,
            BASE_64_FLAGS
        )

        editor.putString(key, data).apply()
    }

    open fun remove(key: String) {
        val editor = prefs.edit()

        editor.remove(key)

        editor.apply()
    }

    private fun verifyKey() {
        if (!keystore!!.available()) {
            keystore.generateKey()
        }
    }
}
