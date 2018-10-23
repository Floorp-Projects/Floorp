/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser.request

import android.content.Context
import mozilla.components.browser.errorpages.ErrorPages
import mozilla.components.browser.errorpages.ErrorType
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.request.RequestInterceptor
import mozilla.components.concept.engine.request.RequestInterceptor.InterceptionResponse
import mozilla.components.concept.engine.request.RequestInterceptor.ErrorResponse

class SampleRequestInterceptor(val context: Context) : RequestInterceptor {
    override fun onLoadRequest(session: EngineSession, uri: String): InterceptionResponse? {
        return when (uri) {
            "sample:about" -> InterceptionResponse.Content("<h1>I am the sample browser</h1>")
            else -> null
        }
    }

    override fun onErrorRequest(
        session: EngineSession,
        errorType: ErrorType,
        uri: String?
    ): ErrorResponse? {
        return ErrorResponse(ErrorPages.createErrorPage(context, errorType, uri))
    }
}
