/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Context
import androidx.annotation.RawRes
import mozilla.components.browser.errorpages.ErrorPages
import mozilla.components.browser.errorpages.ErrorType
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.request.RequestInterceptor
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings

class AppRequestInterceptor(private val context: Context) : RequestInterceptor {
    override fun onLoadRequest(
        session: EngineSession,
        uri: String
    ): RequestInterceptor.InterceptionResponse? {
        adjustTrackingProtection(context, session)
        // WebChannel-driven authentication does not require a separate redirect interceptor.
        return if (context.isInExperiment(Experiments.asFeatureWebChannelsDisabled)) {
            context.components.services.accountsAuthFeature.interceptor.onLoadRequest(session, uri)
        } else {
            null
        }
    }

    private fun adjustTrackingProtection(context: Context, session: EngineSession) {
        val trackingProtectionEnabled = context.settings().shouldUseTrackingProtection
        if (!trackingProtectionEnabled) {
            session.disableTrackingProtection()
        } else {
            val core = context.components.core
            val policy = core.createTrackingProtectionPolicy(normalMode = true)
            core.engine.settings.trackingProtectionPolicy = policy
            session.enableTrackingProtection(policy)
        }
    }

    override fun onErrorRequest(
        session: EngineSession,
        errorType: ErrorType,
        uri: String?
    ): RequestInterceptor.ErrorResponse? {
        val riskLevel = getRiskLevel(errorType)

        context.components.analytics.metrics.track(Event.ErrorPageVisited(errorType))

        return RequestInterceptor.ErrorResponse(
            ErrorPages.createErrorPage(
                context,
                errorType,
                uri = uri,
                htmlResource = riskLevel.htmlRes,
                cssResource = riskLevel.cssRes
            )
        )
    }

    private fun getRiskLevel(errorType: ErrorType): RiskLevel = when (errorType) {
        ErrorType.UNKNOWN,
        ErrorType.ERROR_NET_INTERRUPT,
        ErrorType.ERROR_NET_TIMEOUT,
        ErrorType.ERROR_CONNECTION_REFUSED,
        ErrorType.ERROR_UNKNOWN_SOCKET_TYPE,
        ErrorType.ERROR_REDIRECT_LOOP,
        ErrorType.ERROR_OFFLINE,
        ErrorType.ERROR_NET_RESET,
        ErrorType.ERROR_UNSAFE_CONTENT_TYPE,
        ErrorType.ERROR_CORRUPTED_CONTENT,
        ErrorType.ERROR_CONTENT_CRASHED,
        ErrorType.ERROR_INVALID_CONTENT_ENCODING,
        ErrorType.ERROR_UNKNOWN_HOST,
        ErrorType.ERROR_MALFORMED_URI,
        ErrorType.ERROR_FILE_NOT_FOUND,
        ErrorType.ERROR_FILE_ACCESS_DENIED,
        ErrorType.ERROR_PROXY_CONNECTION_REFUSED,
        ErrorType.ERROR_UNKNOWN_PROXY_HOST,
        ErrorType.ERROR_NO_INTERNET,
        ErrorType.ERROR_UNKNOWN_PROTOCOL -> RiskLevel.Low

        ErrorType.ERROR_SECURITY_BAD_CERT,
        ErrorType.ERROR_SECURITY_SSL,
        ErrorType.ERROR_PORT_BLOCKED -> RiskLevel.Medium

        ErrorType.ERROR_SAFEBROWSING_HARMFUL_URI,
        ErrorType.ERROR_SAFEBROWSING_MALWARE_URI,
        ErrorType.ERROR_SAFEBROWSING_PHISHING_URI,
        ErrorType.ERROR_SAFEBROWSING_UNWANTED_URI -> RiskLevel.High
    }

    private enum class RiskLevel(@RawRes val htmlRes: Int, @RawRes val cssRes: Int) {
        Low(R.raw.low_risk_error_pages, R.raw.low_and_medium_risk_error_style),
        Medium(R.raw.medium_and_high_risk_error_pages, R.raw.low_and_medium_risk_error_style),
        High(R.raw.medium_and_high_risk_error_pages, R.raw.high_risk_error_style),
    }
}
