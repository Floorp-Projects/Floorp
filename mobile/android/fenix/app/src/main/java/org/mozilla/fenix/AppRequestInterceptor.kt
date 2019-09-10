/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Context
import mozilla.components.browser.errorpages.ErrorPages
import mozilla.components.browser.errorpages.ErrorType
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.request.RequestInterceptor
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.exceptions.ExceptionDomains
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import java.net.MalformedURLException
import java.net.URL

class AppRequestInterceptor(private val context: Context) : RequestInterceptor {
    override fun onLoadRequest(session: EngineSession, uri: String): RequestInterceptor.InterceptionResponse? {
        val host = try {
            URL(uri).host
        } catch (e: MalformedURLException) {
            uri
        }

        adjustTrackingProtection(host, context, session)
        // Accounts uses interception to check for a "success URL" in the sign-in flow to finalize authentication.
        return context.components.services.accountsAuthFeature.interceptor.onLoadRequest(session, uri)
    }

    private fun adjustTrackingProtection(host: String, context: Context, session: EngineSession) {
        val trackingProtectionException = ExceptionDomains.load(context).contains(host)
        val trackingProtectionEnabled = context.settings.shouldUseTrackingProtection
        if (trackingProtectionException || !trackingProtectionEnabled) {
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
        val htmlResource = getPageForRiskLevel(riskLevel)
        val cssResource = getStyleForRiskLevel(riskLevel)

        context.components.analytics.metrics.track(Event.ErrorPageVisited(errorType))
        return RequestInterceptor.ErrorResponse(
            ErrorPages
                .createErrorPage(context, errorType, uri = uri, htmlResource = htmlResource, cssResource = cssResource)
        )
    }

    private fun getPageForRiskLevel(riskLevel: RiskLevel): Int {
        return when (riskLevel) {
            RiskLevel.Low -> R.raw.low_risk_error_pages
            RiskLevel.Medium -> R.raw.medium_and_high_risk_error_pages
            RiskLevel.High -> R.raw.medium_and_high_risk_error_pages
        }
    }

    private fun getStyleForRiskLevel(riskLevel: RiskLevel): Int {
        return when (riskLevel) {
            RiskLevel.Low -> R.raw.low_and_medium_risk_error_style
            RiskLevel.Medium -> R.raw.low_and_medium_risk_error_style
            RiskLevel.High -> R.raw.high_risk_error_style
        }
    }

    private fun getRiskLevel(errorType: ErrorType): RiskLevel {
        return when (errorType) {
            // Low risk errors
            ErrorType.UNKNOWN -> RiskLevel.Low
            ErrorType.ERROR_NET_INTERRUPT -> RiskLevel.Low
            ErrorType.ERROR_NET_TIMEOUT -> RiskLevel.Low
            ErrorType.ERROR_CONNECTION_REFUSED -> RiskLevel.Low
            ErrorType.ERROR_UNKNOWN_SOCKET_TYPE -> RiskLevel.Low
            ErrorType.ERROR_REDIRECT_LOOP -> RiskLevel.Low
            ErrorType.ERROR_OFFLINE -> RiskLevel.Low
            ErrorType.ERROR_NET_RESET -> RiskLevel.Low
            ErrorType.ERROR_UNSAFE_CONTENT_TYPE -> RiskLevel.Low
            ErrorType.ERROR_CORRUPTED_CONTENT -> RiskLevel.Low
            ErrorType.ERROR_CONTENT_CRASHED -> RiskLevel.Low
            ErrorType.ERROR_INVALID_CONTENT_ENCODING -> RiskLevel.Low
            ErrorType.ERROR_UNKNOWN_HOST -> RiskLevel.Low
            ErrorType.ERROR_MALFORMED_URI -> RiskLevel.Low
            ErrorType.ERROR_FILE_NOT_FOUND -> RiskLevel.Low
            ErrorType.ERROR_FILE_ACCESS_DENIED -> RiskLevel.Low
            ErrorType.ERROR_PROXY_CONNECTION_REFUSED -> RiskLevel.Low
            ErrorType.ERROR_UNKNOWN_PROXY_HOST -> RiskLevel.Low
            ErrorType.ERROR_UNKNOWN_PROTOCOL -> RiskLevel.Low

            // Medium risk errors
            ErrorType.ERROR_SECURITY_BAD_CERT -> RiskLevel.Medium
            ErrorType.ERROR_SECURITY_SSL -> RiskLevel.Medium
            ErrorType.ERROR_PORT_BLOCKED -> RiskLevel.Medium

            // High risk errors
            ErrorType.ERROR_SAFEBROWSING_HARMFUL_URI -> RiskLevel.High
            ErrorType.ERROR_SAFEBROWSING_MALWARE_URI -> RiskLevel.High
            ErrorType.ERROR_SAFEBROWSING_PHISHING_URI -> RiskLevel.High
            ErrorType.ERROR_SAFEBROWSING_UNWANTED_URI -> RiskLevel.High
        }
    }

    sealed class RiskLevel {
        object Low : RiskLevel()
        object Medium : RiskLevel()
        object High : RiskLevel()
    }
}
