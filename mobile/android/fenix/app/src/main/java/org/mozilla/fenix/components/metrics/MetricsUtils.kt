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
    // The number of iterations to compute the hash. RFC 2898 suggests
    // a minimum of 1000 iterations.
    private const val PBKDF2_ITERATIONS = 1000
    private const val PBKDF2_KEY_LEN_BITS = 256

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
        searchAccessPoint: Source,
    ) {
        val identifier = if (engine.type == SearchEngine.Type.CUSTOM) "custom" else engine.id.lowercase()
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
     * Get the default salt to use for hashing. This is a convenience
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
            Logger.debug("getAdvertisingID() - Google Play not installed on the device")
            null
        } catch (e: GooglePlayServicesRepairableException) {
            Logger.debug("getAdvertisingID() - recoverable error connecting to Google Play Services")
            null
        } catch (e: IllegalStateException) {
            // This is unlikely to happen, as this should be running off the main thread.
            Logger.debug("getAdvertisingID() - AdvertisingIdClient must be called off the main thread")
            null
        } catch (e: IOException) {
            Logger.debug("getAdvertisingID() - unable to connect to Google Play Services")
            null
        } catch (e: NullPointerException) {
            Logger.debug("getAdvertisingID() - no Google Advertising ID available")
            null
        }
    }

    /**
     * Produces a hashed version of the Google Advertising ID.
     * We want users using more than one of our products to report a different
     * ID in each of them. This function runs off the main thread and is CPU-bound.
     *
     * The `customSalt` parameter allows for dynamic setting of the salt for
     * certain experiments or any one-off applications.
     *
     * @return an hashed and salted Google Advertising ID or null if it was not possible
     *         to get the Google Advertising ID.
     */
    suspend fun getHashedIdentifier(context: Context, customSalt: String? = null): String? =
        withContext(Dispatchers.Default) {
            getAdvertisingID(context)?.let { unhashedID ->
                // Add some salt to the ID, before hashing. We have a default salt that is used for
                // all the hashes unless you specifically provide something different. This is done
                // to stabalize all hashing within a single product. The customSalt allows for tweaking
                // in case there are specific use-cases that require something custom.
                val salt = customSalt ?: getHashingSalt()

                // Apply hashing.
                try {
                    // Note that we intentionally want to use slow hashing functions here in order
                    // to increase the cost of potentially repeatedly guess the original unhashed
                    // identifier.
                    val keySpec = PBEKeySpec(
                        unhashedID.toCharArray(),
                        salt.toByteArray(),
                        PBKDF2_ITERATIONS,
                        PBKDF2_KEY_LEN_BITS,
                    )

                    val keyFactory = SecretKeyFactory.getInstance("PBKDF2WithHmacSHA1")
                    val hashedBytes = keyFactory.generateSecret(keySpec).encoded
                    Base64.encodeToString(hashedBytes, Base64.NO_WRAP)
                } catch (e: java.lang.NullPointerException) {
                    Logger.error("getHashedIdentifier() - missing or wrong salt parameter")
                    null
                } catch (e: IllegalArgumentException) {
                    Logger.error("getHashedIdentifier() - wrong parameter", e)
                    null
                } catch (e: NoSuchAlgorithmException) {
                    Logger.error("getHashedIdentifier() - algorithm not available")
                    null
                } catch (e: InvalidKeySpecException) {
                    Logger.error("getHashedIdentifier() - invalid key spec")
                    null
                }
            }
        }
}
