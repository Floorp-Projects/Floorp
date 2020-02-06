/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.content.Context
import android.content.SharedPreferences
import android.util.Base64
import androidx.annotation.VisibleForTesting
import com.google.android.gms.ads.identifier.AdvertisingIdClient
import com.google.android.gms.common.GooglePlayServicesNotAvailableException
import com.google.android.gms.common.GooglePlayServicesRepairableException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.GleanMetrics.Activation
import org.mozilla.fenix.GleanMetrics.Pings
import java.io.IOException
import java.security.NoSuchAlgorithmException
import java.security.spec.InvalidKeySpecException
import javax.crypto.SecretKeyFactory
import javax.crypto.spec.PBEKeySpec

class ActivationPing(private val context: Context) {
    companion object {
        // The number of iterations to compute the hash. RFC 2898 suggests
        // a minimum of 1000 iterations.
        const val PBKDF2_ITERATIONS = 1000
        const val PBKDF2_KEY_LEN_BITS = 256
    }

    private val prefs: SharedPreferences by lazy {
        context.getSharedPreferences(
            "${this.javaClass.canonicalName}.prefs", Context.MODE_PRIVATE)
    }

    /**
     * Checks whether or not the activation ping was already
     * triggered by the application.
     *
     * Note that this only tells us that Fenix triggered the
     * ping and then delegated the transmission to Glean. We
     * have no way to tell if it was actually sent or not.
     *
     * @return true if it was already triggered, false otherwise.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun wasAlreadyTriggered(): Boolean {
        return prefs.getBoolean("ping_sent", false)
    }

    /**
     * Marks the "activation" ping as triggered by the application.
     * This ensures the ping is not triggered again at the next app
     * start.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun markAsTriggered() {
        prefs.edit().putBoolean("ping_sent", true).apply()
    }

    /**
     * Query the Google Advertising API to get the Google Advertising ID.
     *
     * This is meant to be used off the main thread. The API will throw an
     * exception and we will print a log message otherwise.
     *
     * @return a String containing the Google Advertising ID or null.
     */
    @Suppress("TooGenericExceptionCaught")
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getAdvertisingID(): String? {
        return try {
            AdvertisingIdClient.getAdvertisingIdInfo(context).id
        } catch (e: GooglePlayServicesNotAvailableException) {
            Logger.debug("ActivationPing - Google Play not installed on the device")
            null
        } catch (e: GooglePlayServicesRepairableException) {
            Logger.debug("ActivationPing - recoverable error connecting to Google Play Services")
            null
        } catch (e: IllegalStateException) {
            // This is unlikely to happen, as this should be running off the main thread.
            Logger.debug("ActivationPing - AdvertisingIdClient must be called off the main thread")
            null
        } catch (e: IOException) {
            Logger.debug("ActivationPing - unable to connect to Google Play Services")
            null
        } catch (e: NullPointerException) {
            Logger.debug("ActivationPing - no Google Advertising ID available")
            null
        }
    }

    /**
     * Get the salt to use for hashing. This is a convenience
     * function to help with unit tests.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getHashingSalt(): String = "org.mozilla.fenix-salt"

    /**
     * Produces an hashed version of the Google Advertising ID.
     * We want users using more than one of our products to report a different
     * ID in each of them. This function runs off the main thread and is CPU-bound.
     *
     * @return an hashed and salted Google Advertising ID or null if it was not possible
     *         to get the Google Advertising ID.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal suspend fun getHashedIdentifier(): String? = withContext(Dispatchers.Default) {
        getAdvertisingID()?.let { unhashedID ->
            // Add some salt to the ID, before hashing. For this specific use-case, it's ok
            // to use the same salt value for all the hashes. We want hashes to be stable
            // within a single product, but we don't want hashes to be the same across different
            // products (e.g. Fennec vs Fenix).
            val salt = getHashingSalt()

            // Apply hashing.
            try {
                // Note that we intentionally want to use slow hashing functions here in order
                // to increase the cost of potentially repeatedly guess the original unhashed
                // identifier.
                val keySpec = PBEKeySpec(
                    unhashedID.toCharArray(),
                    salt.toByteArray(),
                    PBKDF2_ITERATIONS,
                    PBKDF2_KEY_LEN_BITS)

                val keyFactory = SecretKeyFactory.getInstance("PBKDF2WithHmacSHA1")
                val hashedBytes = keyFactory.generateSecret(keySpec).encoded
                Base64.encodeToString(hashedBytes, Base64.NO_WRAP)
            } catch (e: java.lang.NullPointerException) {
                Logger.error("ActivationPing - missing or wrong salt parameter")
                null
            } catch (e: IllegalArgumentException) {
                Logger.error("ActivationPing - wrong parameter", e)
                null
            } catch (e: NoSuchAlgorithmException) {
                Logger.error("ActivationPing - algorithm not available")
                null
            } catch (e: InvalidKeySpecException) {
                Logger.error("ActivationPing - invalid key spec")
                null
            }
        }
    }

    /**
     * Fills the metrics and triggers the 'activation' ping.
     * This is a separate function to simplify unit-testing.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun triggerPing() {
        // Generate the activation_id.
        Activation.activationId.generateAndSet()

        CoroutineScope(Dispatchers.IO).launch {
            val hashedId = getHashedIdentifier()
            if (hashedId != null) {
                Logger.info("ActivationPing - generating ping with the hashed id")
                // We have a valid, hashed Google Advertising ID.
                Activation.identifier.set(hashedId)
            }

            Logger.info("ActivationPing - generating ping (has `identifier`: ${hashedId != null})")
            Pings.activation.submit()
            markAsTriggered()
        }
    }

    /**
     * Trigger sending the `activation` ping if it wasn't sent already.
     * Then, mark it so that it doesn't get triggered next time Fenix
     * starts.
     */
    fun checkAndSend() {
        if (wasAlreadyTriggered()) {
            Logger.debug("ActivationPing - already generated")
            return
        }

        triggerPing()
    }
}
