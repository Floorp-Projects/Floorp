/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Context
import android.net.ConnectivityManager
import androidx.core.content.getSystemService
import androidx.navigation.NavController
import mozilla.components.browser.errorpages.ErrorPages
import mozilla.components.browser.errorpages.ErrorType
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.request.RequestInterceptor
import org.mozilla.fenix.GleanMetrics.ErrorPage
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.isOnline
import org.mozilla.fenix.helpers.TestHelper.appContext
import java.lang.ref.WeakReference

/**
 * This class overrides the application's request interceptor to
 * deactivate the FxA web channel
 * which is not supported on the staging servers.
 */
class AppRequestInterceptor(private val context: Context) : RequestInterceptor {

    private var navController: WeakReference<NavController>? = null

    fun setNavigationController(navController: NavController) {
        this.navController = WeakReference(navController)
    }

    override fun onLoadRequest(
        engineSession: EngineSession,
        uri: String,
        lastUri: String?,
        hasUserGesture: Boolean,
        isSameDomain: Boolean,
        isRedirect: Boolean,
        isDirectNavigation: Boolean,
        isSubframeRequest: Boolean
    ): RequestInterceptor.InterceptionResponse? {

        interceptFxaRequest(
            engineSession,
            uri,
            lastUri,
            hasUserGesture,
            isSameDomain,
            isRedirect,
            isDirectNavigation,
            isSubframeRequest
        )?.let { response ->
            return response
        }

        interceptAmoRequest(uri, isSameDomain, hasUserGesture)?.let { response ->
            return response
        }

        return context.components.services.appLinksInterceptor
            .onLoadRequest(
                engineSession,
                uri,
                lastUri,
                hasUserGesture,
                isSameDomain,
                isRedirect,
                isDirectNavigation,
                isSubframeRequest
            )
    }

    override fun onErrorRequest(
        session: EngineSession,
        errorType: ErrorType,
        uri: String?
    ): RequestInterceptor.ErrorResponse? {
        val improvedErrorType = improveErrorType(errorType)
        val riskLevel = getRiskLevel(improvedErrorType)

        ErrorPage.visitedError.record(ErrorPage.VisitedErrorExtra(improvedErrorType.name))

        val errorPageUri = ErrorPages.createUrlEncodedErrorPage(
            context = context,
            errorType = improvedErrorType,
            uri = uri,
            htmlResource = riskLevel.htmlRes
        )

        return RequestInterceptor.ErrorResponse(errorPageUri)
    }

    /**
     * Checks if the provided [uri] is a request to install an add-on from addons.mozilla.org and
     * redirects to Add-ons Manager to trigger installation if needed.
     *
     * @return [RequestInterceptor.InterceptionResponse.Deny] when installation was triggered and
     * the original request can be skipped, otherwise null to continue loading the page.
     */
    private fun interceptAmoRequest(
        uri: String,
        isSameDomain: Boolean,
        hasUserGesture: Boolean
    ): RequestInterceptor.InterceptionResponse? {
        // First we execute a quick check to see if this is a request we're interested in i.e. a
        // request triggered by the user and coming from AMO.
        if (hasUserGesture && isSameDomain && uri.startsWith(AMO_BASE_URL)) {

            // Check if this is a request to install an add-on.
            val matchResult = AMO_INSTALL_URL_REGEX.toRegex().matchEntire(uri)
            if (matchResult != null) {

                // Navigate and trigger add-on installation.
                matchResult.groupValues.getOrNull(1)?.let { addonId ->
                    navController?.get()?.navigate(
                        NavGraphDirections.actionGlobalAddonsManagementFragment(addonId)
                    )

                    // We've redirected to the add-ons management fragment, skip original request.
                    return RequestInterceptor.InterceptionResponse.Deny
                }
            }
        }

        // In all other case we let the original request proceed.
        return null
    }

    @Suppress("LongParameterList")
    private fun interceptFxaRequest(
        engineSession: EngineSession,
        uri: String,
        lastUri: String?,
        hasUserGesture: Boolean,
        isSameDomain: Boolean,
        isRedirect: Boolean,
        isDirectNavigation: Boolean,
        isSubframeRequest: Boolean
    ): RequestInterceptor.InterceptionResponse? {
        return appContext.components.services.accountsAuthFeature.interceptor.onLoadRequest(
            engineSession,
            uri,
            lastUri,
            hasUserGesture,
            isSameDomain,
            isRedirect,
            isDirectNavigation,
            isSubframeRequest
        )
    }

    /**
     * Where possible, this will make the error type more accurate by including information not
     * available to AC.
     */
    private fun improveErrorType(errorType: ErrorType): ErrorType {
        // This is not an ideal solution. For context, see:
        // https://github.com/mozilla-mobile/android-components/pull/5068#issuecomment-558415367

        val isConnected: Boolean = context.getSystemService<ConnectivityManager>()!!.isOnline()

        return when {
            errorType == ErrorType.ERROR_UNKNOWN_HOST && !isConnected -> ErrorType.ERROR_NO_INTERNET
            else -> errorType
        }
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
        ErrorType.ERROR_HTTPS_ONLY,
        ErrorType.ERROR_BAD_HSTS_CERT,
        ErrorType.ERROR_UNKNOWN_PROTOCOL -> RiskLevel.Low

        ErrorType.ERROR_SECURITY_BAD_CERT,
        ErrorType.ERROR_SECURITY_SSL,
        ErrorType.ERROR_PORT_BLOCKED -> RiskLevel.Medium

        ErrorType.ERROR_SAFEBROWSING_HARMFUL_URI,
        ErrorType.ERROR_SAFEBROWSING_MALWARE_URI,
        ErrorType.ERROR_SAFEBROWSING_PHISHING_URI,
        ErrorType.ERROR_SAFEBROWSING_UNWANTED_URI -> RiskLevel.High
    }

    internal enum class RiskLevel(val htmlRes: String) {
        Low(LOW_AND_MEDIUM_RISK_ERROR_PAGES),
        Medium(LOW_AND_MEDIUM_RISK_ERROR_PAGES),
        High(HIGH_RISK_ERROR_PAGES),
    }

    companion object {
        internal const val LOW_AND_MEDIUM_RISK_ERROR_PAGES = "low_and_medium_risk_error_pages.html"
        internal const val HIGH_RISK_ERROR_PAGES = "high_risk_error_pages.html"
        internal const val AMO_BASE_URL = BuildConfig.AMO_BASE_URL
        internal const val AMO_INSTALL_URL_REGEX = "$AMO_BASE_URL/android/downloads/file/([^\\s]+)/([^\\s]+\\.xpi)"
    }
}
