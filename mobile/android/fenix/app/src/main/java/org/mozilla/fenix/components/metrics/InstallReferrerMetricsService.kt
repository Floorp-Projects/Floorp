/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.content.Context
import android.net.UrlQuerySanitizer
import android.os.RemoteException
import androidx.annotation.VisibleForTesting
import com.android.installreferrer.api.InstallReferrerClient
import com.android.installreferrer.api.InstallReferrerStateListener
import org.mozilla.fenix.GleanMetrics.PlayStoreAttribution
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.utils.Settings

/**
 * A metrics service used to derive the UTM parameters with the Google Play Install Referrer library.
 *
 * At first startup, the [UTMParams] are derived from the install referrer URL and stored in settings.
 */
class InstallReferrerMetricsService(private val context: Context) : MetricsService {
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
                            try {
                                val response = client.installReferrer
                                recordInstallReferrer(context.settings(), response.installReferrer)
                                context.settings().utmParamsKnown = true
                            } catch (e: RemoteException) {
                                // NOOP.
                                // We can't do anything about this.
                            }
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

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun recordInstallReferrer(settings: Settings, url: String?) {
        val params = url?.let(UTMParams::fromUrl)
        if (params == null || params.isEmpty()) {
            return
        }
        params.intoSettings(settings)

        params.apply {
            source?.let {
                PlayStoreAttribution.source.set(it)
            }
            medium?.let {
                PlayStoreAttribution.medium.set(it)
            }
            campaign?.let {
                PlayStoreAttribution.campaign.set(it)
            }
            content?.let {
                PlayStoreAttribution.content.set(it)
            }
            term?.let {
                PlayStoreAttribution.term.set(it)
            }
        }
    }
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
    val source: String?,
    val medium: String?,
    val campaign: String?,
    val term: String?,
    val content: String?,
) {
    companion object {
        const val UTM_SOURCE = "utm_source"
        const val UTM_MEDIUM = "utm_medium"
        const val UTM_CAMPAIGN = "utm_campaign"
        const val UTM_TERM = "utm_term"
        const val UTM_CONTENT = "utm_content"

        /**
         * Derive a set of UTM parameters from a string URL.
         */
        fun fromUrl(url: String): UTMParams =
            with(UrlQuerySanitizer()) {
                allowUnregisteredParamaters = true
                unregisteredParameterValueSanitizer = UrlQuerySanitizer.getUrlAndSpaceLegal()
                parseUrl(url)
                UTMParams(
                    source = getValue(UTM_SOURCE),
                    medium = getValue(UTM_MEDIUM),
                    campaign = getValue(UTM_CAMPAIGN),
                    term = getValue(UTM_TERM),
                    content = getValue(UTM_CONTENT),
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
                    term = utmTerm,
                    content = utmContent,
                )
            }
    }

    /**
     * Persist the UTM params into Settings.
     */
    fun intoSettings(settings: Settings) {
        with(settings) {
            utmSource = source ?: ""
            utmMedium = medium ?: ""
            utmCampaign = campaign ?: ""
            utmTerm = term ?: ""
            utmContent = content ?: ""
        }
    }

    /**
     * Return [true] if none of the utm params are set.
     */
    fun isEmpty(): Boolean {
        return source.isNullOrBlank() &&
            medium.isNullOrBlank() &&
            campaign.isNullOrBlank() &&
            term.isNullOrBlank() &&
            content.isNullOrBlank()
    }
}
