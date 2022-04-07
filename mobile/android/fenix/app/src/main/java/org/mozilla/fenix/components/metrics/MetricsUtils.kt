/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.content.Context
import android.util.Base64
import androidx.annotation.VisibleForTesting
import com.google.android.gms.ads.identifier.AdvertisingIdClient
import com.google.android.gms.common.GooglePlayServicesNotAvailableException
import com.google.android.gms.common.GooglePlayServicesRepairableException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.Metrics
import java.io.IOException
import java.security.NoSuchAlgorithmException
import java.security.spec.InvalidKeySpecException
import javax.crypto.SecretKeyFactory
import javax.crypto.spec.PBEKeySpec

object MetricsUtils {

    /**
     * Possible sources for a performed search.
     */
    enum class Source {
        ACTION, SHORTCUT, SUGGESTION, TOPSITE, WIDGET, NONE
    }

    /**
     * Records the appropriate metric for performed searches.
     * @engine the engine used for searching.
     * @isDefault whether te engine is the default engine or not.
     * @searchAccessPoint the source of the search. Can be one of the values of [Source].
     */
    fun recordSearchMetrics(
        engine: SearchEngine,
        isDefault: Boolean,
        searchAccessPoint: Source
    ) {
        val identifier = if (engine.type == SearchEngine.Type.CUSTOM) "custom" else engine.id
        val source = searchAccessPoint.name.lowercase()

        Metrics.searchCount["$identifier.$source"].add()

        val performedSearchExtra = if (isDefault) {
            "default.$source"
        } else {
            "shortcut.$source"
        }

        Events.performedSearch.record(Events.PerformedSearchExtra(performedSearchExtra))
    }

    /**
     * Get the salt to use for hashing. This is a convenience
     * function to help with unit tests.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getHashingSalt(): String = "org.mozilla.fenix-salt"

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
    internal fun getAdvertisingID(context: Context): String? {
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
     * Produces a hashed version of the Google Advertising ID.
     * We want users using more than one of our products to report a different
     * ID in each of them. This function runs off the main thread and is CPU-bound.
     *
     * @return an hashed and salted Google Advertising ID or null if it was not possible
     *         to get the Google Advertising ID.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    suspend fun getHashedIdentifier(context: Context): String? = withContext(Dispatchers.Default) {
        getAdvertisingID(context)?.let { unhashedID ->
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
                    ActivationPing.PBKDF2_ITERATIONS,
                    ActivationPing.PBKDF2_KEY_LEN_BITS
                )

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
}
