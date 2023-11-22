/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.request

import android.content.Intent
import mozilla.components.browser.errorpages.ErrorType
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags

/**
 * Interface for classes that want to intercept load requests to allow custom behavior.
 */
interface RequestInterceptor {

    /**
     * An alternative response for an intercepted request.
     */
    sealed class InterceptionResponse {
        data class Content(
            val data: String,
            val mimeType: String = "text/html",
            val encoding: String = "UTF-8",
        ) : InterceptionResponse()

        /**
         * The intercepted request URL to load.
         *
         * @param url The URL of the request.
         * @param flags The [LoadUrlFlags] to use when loading the provided [url].
         * @param additionalHeaders The extra headers to use when loading the provided [url].
         */
        data class Url(
            val url: String,
            val flags: LoadUrlFlags = LoadUrlFlags.select(
                LoadUrlFlags.EXTERNAL,
                LoadUrlFlags.LOAD_FLAGS_BYPASS_LOAD_URI_DELEGATE,
            ),
            val additionalHeaders: Map<String, String>? = null,
        ) : InterceptionResponse()

        data class AppIntent(val appIntent: Intent, val url: String) : InterceptionResponse()

        /**
         * Deny request without further action.
         */
        object Deny : InterceptionResponse()
    }

    /**
     * An alternative response for an error request.
     * Used to load an encoded URI directly.
     */
    data class ErrorResponse(val uri: String)

    /**
     * A request to open an URI. This is called before each page load to allow
     * providing custom behavior.
     *
     * @param engineSession The engine session that initiated the callback.
     * @param uri The URI of the request.
     * @param lastUri The URI of the last request.
     * @param hasUserGesture If the request is triggered by the user then true, else false.
     * @param isSameDomain If the request is the same domain as the current URL then true, else false.
     * @param isRedirect If the request is due to a redirect then true, else false.
     * @param isDirectNavigation If the request is due to a direct navigation then true, else false.
     * @param isSubframeRequest If the request is coming from a subframe then true, else false.
     * @return An [InterceptionResponse] object containing alternative content
     * or an alternative URL. Null if the original request should continue to
     * be loaded.
     */
    @Suppress("LongParameterList")
    fun onLoadRequest(
        engineSession: EngineSession,
        uri: String,
        lastUri: String?,
        hasUserGesture: Boolean,
        isSameDomain: Boolean,
        isRedirect: Boolean,
        isDirectNavigation: Boolean,
        isSubframeRequest: Boolean,
    ): InterceptionResponse? = null

    /**
     * A request that the engine wasn't able to handle that resulted in an error.
     *
     * @param session The engine session that initiated the callback.
     * @param errorType The error that was provided by the engine related to the
     * type of error caused.
     * @param uri The uri that resulted in the error.
     * @return An [ErrorResponse] object containing content to display for the
     * provided error type.
     */
    fun onErrorRequest(session: EngineSession, errorType: ErrorType, uri: String?): ErrorResponse? = null

    /**
     * Returns whether or not this [RequestInterceptor] should intercept load
     * requests initiated by the app (via direct calls to [EngineSession.loadUrl]).
     * All other requests triggered by users interacting with web content
     * (e.g. following links) or redirects will always be intercepted.
     *
     * @return true if app initiated requests should be intercepted,
     * otherwise false. Defaults to false.
     */
    fun interceptsAppInitiatedRequests() = false
}
