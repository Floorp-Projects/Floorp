/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.content.Context
import android.os.RemoteException
import com.android.installreferrer.api.InstallReferrerClient
import com.android.installreferrer.api.InstallReferrerStateListener
import mozilla.components.support.base.log.logger.Logger
import org.json.JSONException
import org.json.JSONObject
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.GleanMetrics.MetaAttribution
import org.mozilla.fenix.GleanMetrics.PlayStoreAttribution
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.utils.Settings

/**
 * A metrics service used to derive the UTM parameters with the Google Play Install Referrer library.
 *
 * At first startup, the [UTMParams] and/or [MetaParams] are derived from the install referrer URL
 * and stored in settings.
 */
class InstallReferrerMetricsService(private val context: Context) : MetricsService {
    private val logger = Logger("InstallReferrerMetricsService")
    override val type = MetricServiceType.Marketing

    private var referrerClient: InstallReferrerClient? = null

    override fun start() {
        if (context.settings().utmParamsKnown) {
            return
        }

        val timerId = PlayStoreAttribution.attributionTime.start()
        val client = InstallReferrerClient.newBuilder(context).build()
        referrerClient = client

        client.startConnection(
            object : InstallReferrerStateListener {
                override fun onInstallReferrerSetupFinished(responseCode: Int) {
                    PlayStoreAttribution.attributionTime.stopAndAccumulate(timerId)
                    when (responseCode) {
                        InstallReferrerClient.InstallReferrerResponse.OK -> {
                            // Connection established.
                            val installReferrerResponse = try {
                                client.installReferrer.installReferrer
                            } catch (e: RemoteException) {
                                // We can't do anything about this.
                                logger.error("Failed to retrieve install referrer response", e)
                                null
                            }

                            if (installReferrerResponse.isNullOrBlank()) {
                                return
                            }

                            PlayStoreAttribution.installReferrerResponse.set(installReferrerResponse)

                            val utmParams = UTMParams.parseUTMParameters(installReferrerResponse)
                            if (FeatureFlags.metaAttributionEnabled) {
                                MetaParams.extractMetaAttribution(utmParams.content)
                                    ?.recordMetaAttribution()
                            }

                            utmParams.recordInstallReferrer(context.settings())
                            context.settings().utmParamsKnown = true
                        }

                        InstallReferrerClient.InstallReferrerResponse.FEATURE_NOT_SUPPORTED -> {
                            // API not available on the current Play Store app.
                            context.settings().utmParamsKnown = true
                        }

                        InstallReferrerClient.InstallReferrerResponse.SERVICE_UNAVAILABLE -> {
                            // Connection couldn't be established.
                        }
                    }
                    // End the connection, and null out the client.
                    stop()
                }

                override fun onInstallReferrerServiceDisconnected() {
                    // Try to restart the connection on the next request to
                    // Google Play by calling the startConnection() method.
                    referrerClient = null
                }
            },
        )
    }

    override fun stop() {
        referrerClient?.endConnection()
        referrerClient = null
    }

    override fun track(event: Event) = Unit

    override fun shouldTrack(event: Event): Boolean = false
}

/**
 * Descriptions of utm parameters comes from
 * https://support.google.com/analytics/answer/1033863
 * - utm_source
 *  Identify the advertiser, site, publication, etc.
 *  that is sending traffic to your property, for example: google, newsletter4, billboard.
 * - utm_medium
 *  The advertising or marketing medium, for example: cpc, banner, email newsletter.
 * utm_campaign
 *  The individual campaign name, slogan, promo code, etc. for a product.
 * - utm_term
 *  Identify paid search keywords.
 *  If you're manually tagging paid keyword campaigns, you should also use
 *  utm_term to specify the keyword.
 * - utm_content
 *  Used to differentiate similar content, or links within the same ad.
 *  For example, if you have two call-to-action links within the same email message,
 *  you can use utm_content and set different values for each so you can tell
 *  which version is more effective.
 */
data class UTMParams(
    val source: String,
    val medium: String,
    val campaign: String,
    val content: String,
    val term: String,
) {

    companion object {
        const val UTM_SOURCE = "utm_source"
        const val UTM_MEDIUM = "utm_medium"
        const val UTM_CAMPAIGN = "utm_campaign"
        const val UTM_CONTENT = "utm_content"
        const val UTM_TERM = "utm_term"

        /**
         * Try and unpack the install referrer response.
         */
        fun parseUTMParameters(installReferrerResponse: String): UTMParams {
            val utmParams = mutableMapOf<String, String>()
            val params = installReferrerResponse.split("&")

            for (param in params) {
                val keyValue = param.split("=")
                if (keyValue.size == 2) {
                    val key = keyValue[0]
                    val value = keyValue[1]
                    utmParams[key] = value
                }
            }

            return UTMParams(
                source = utmParams[UTM_SOURCE] ?: "",
                medium = utmParams[UTM_MEDIUM] ?: "",
                campaign = utmParams[UTM_CAMPAIGN] ?: "",
                content = utmParams[UTM_CONTENT] ?: "",
                term = utmParams[UTM_TERM] ?: "",
            )
        }

        /**
         * Derive the set of UTM parameters stored in Settings.
         */
        fun fromSettings(settings: Settings): UTMParams =
            with(settings) {
                UTMParams(
                    source = utmSource,
                    medium = utmMedium,
                    campaign = utmCampaign,
                    content = utmContent,
                    term = utmTerm,
                )
            }
    }

    /**
     * Persist the UTM params into Settings.
     */
    fun intoSettings(settings: Settings) {
        with(settings) {
            utmSource = source
            utmMedium = medium
            utmCampaign = campaign
            utmTerm = term
            utmContent = content
        }
    }

    /**
     * Check if this UTM param is empty
     *
     * @Return [Boolean] true if none of the utm params are set.
     */
    fun isEmpty(): Boolean {
        return source.isBlank() &&
            medium.isBlank() &&
            campaign.isBlank() &&
            term.isBlank() &&
            content.isBlank()
    }

    /**
     * record UTM params into settings and telemetry
     *
     * @param settings [Settings] application settings.
     */
    fun recordInstallReferrer(settings: Settings) {
        if (isEmpty()) {
            return
        }
        intoSettings(settings)

        PlayStoreAttribution.source.set(source)
        PlayStoreAttribution.medium.set(medium)
        PlayStoreAttribution.campaign.set(campaign)
        PlayStoreAttribution.content.set(content)
        PlayStoreAttribution.term.set(term)
    }
}

/**
 * Descriptions of Meta attribution parameters comes from
 * https://developers.facebook.com/docs/marketing-api/reference/ad-campaign#fields
 *
 * @property app the ID of application in the referrer response.
 * @property t the value of user interaction in the referrer response.
 * @property data the encrypted data in the referrer response.
 * @property nonce the nonce for decrypting [data] in the referrer response.
 */
data class MetaParams(
    val app: String,
    val t: String,
    val data: String,
    val nonce: String,
) {
    companion object {
        private val logger = Logger("MetaParams")
        private const val APP = "app"
        private const val T = "t"
        private const val SOURCE = "source"
        private const val DATA = "data"
        private const val NONCE = "nonce"

        @Suppress("ReturnCount")
        internal fun extractMetaAttribution(contentString: String?): MetaParams? {
            if (contentString == null) {
                return null
            }

            val data: String
            val nonce: String

            val contentJson = try {
                JSONObject(contentString)
            } catch (e: JSONException) {
                logger.error("content is not JSON", e)
                // can't recover from this
                return null
            }

            val app = try {
                contentJson.optString(APP) ?: ""
            } catch (e: JSONException) {
                logger.error("failed to extract app", e)
                // this is an acceptable outcome
                ""
            }

            val t = try {
                contentJson.optString(T) ?: ""
            } catch (e: JSONException) {
                logger.error("failed to extract t", e)
                // this is an acceptable outcome
                ""
            }

            try {
                val source = contentJson.optJSONObject(SOURCE)
                data = source?.optString(DATA) ?: ""
                nonce = source?.optString(NONCE) ?: ""
            } catch (e: JSONException) {
                logger.error("failed to extract data or nonce", e)
                // can't recover from this
                return null
            }

            if (data.isBlank() || nonce.isBlank()) {
                return null
            }

            return MetaParams(
                app = app,
                t = t,
                data = data,
                nonce = nonce,
            )
        }
    }

    /**
     * record META attribution params to telemetry
     */
    fun recordMetaAttribution() {
        MetaAttribution.app.set(app)
        MetaAttribution.t.set(t)
        MetaAttribution.data.set(data)
        MetaAttribution.nonce.set(nonce)
    }
}
