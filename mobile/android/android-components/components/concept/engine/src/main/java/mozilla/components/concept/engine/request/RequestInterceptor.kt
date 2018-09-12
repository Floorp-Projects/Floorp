/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.request

import mozilla.components.concept.engine.EngineSession

/**
 * Interface for classes that want to intercept load requests to allow custom behavior.
 */
interface RequestInterceptor {
    /**
     * An alternative response for an intercepted request.
     */
    data class InterceptionResponse(
        val data: String,
        val mimeType: String = "text/html",
        val encoding: String = "UTF-8"
    )

    /**
     * A request to open an URI. This is called before each page load to allow custom behavior implementation.
     *
     * @param session The engine session that initiated the callback.
     * @return An InterceptionResponse object containing alternative content if the request should be intercepted.
     *         <code>null</code> otherwise.
     */
    fun onLoadRequest(session: EngineSession, uri: String): InterceptionResponse? = null

    /**
     * A request that the engine wasn't able to handle that resulted in an error.
     *
     * @param session The engine session that initiated the callback.
     * @param errorCode The error code that was provided by the engine related to the type of error caused.
     * @param uri The uri that resulted in the error.
     */
    fun onErrorRequest(session: EngineSession, errorCode: Int, uri: String?) = Unit
}
