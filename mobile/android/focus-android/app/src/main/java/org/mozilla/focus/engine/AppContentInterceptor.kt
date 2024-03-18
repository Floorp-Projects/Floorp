/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.engine

import android.content.Context
import android.content.Intent
import android.content.Intent.FLAG_ACTIVITY_NEW_TASK
import mozilla.components.browser.errorpages.ErrorPages
import mozilla.components.browser.errorpages.ErrorType
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.request.RequestInterceptor
import org.mozilla.focus.R
import org.mozilla.focus.activity.CrashListActivity
import org.mozilla.focus.browser.LocalizedContent
import org.mozilla.focus.ext.components
import org.mozilla.focus.utils.SupportUtils

class AppContentInterceptor(
    private val context: Context,
) : RequestInterceptor {
    override fun onLoadRequest(
        engineSession: EngineSession,
        uri: String,
        lastUri: String?,
        hasUserGesture: Boolean,
        isSameDomain: Boolean,
        isRedirect: Boolean,
        isDirectNavigation: Boolean,
        isSubframeRequest: Boolean,
    ): RequestInterceptor.InterceptionResponse? {
        return when (uri) {
            LocalizedContent.URL_ABOUT -> RequestInterceptor.InterceptionResponse.Content(
                LocalizedContent.loadAbout(context),
                encoding = "base64",
            )

            LocalizedContent.URL_RIGHTS -> RequestInterceptor.InterceptionResponse.Content(
                LocalizedContent.loadRights(context),
                encoding = "base64",
            )

            LocalizedContent.URL_GPL -> RequestInterceptor.InterceptionResponse.Content(
                LocalizedContent.loadGPL(context),
                encoding = "base64",
            )

            LocalizedContent.URL_LICENSES -> RequestInterceptor.InterceptionResponse.Content(
                LocalizedContent.loadLicenses(context),
                encoding = "base64",
            )

            "about:crashes" -> {
                val intent = Intent(context, CrashListActivity::class.java)
                intent.addFlags(FLAG_ACTIVITY_NEW_TASK)
                context.startActivity(intent)

                RequestInterceptor.InterceptionResponse.Url("about:blank")
            }

            else -> context.components.appLinksInterceptor.onLoadRequest(
                engineSession,
                uri,
                lastUri,
                hasUserGesture,
                isSameDomain,
                isRedirect,
                isDirectNavigation,
                isSubframeRequest,
            )
        }
    }

    override fun onErrorRequest(
        session: EngineSession,
        errorType: ErrorType,
        uri: String?,
    ): RequestInterceptor.ErrorResponse {
        val errorPage = ErrorPages.createUrlEncodedErrorPage(
            context,
            errorType,
            uri,
            titleOverride = { type -> getErrorPageTitle(context, type) },
            descriptionOverride = { type -> getErrorPageDescription(context, type) },
        )
        return RequestInterceptor.ErrorResponse(errorPage)
    }

    override fun interceptsAppInitiatedRequests() = true
}

private fun getErrorPageTitle(context: Context, type: ErrorType): String? {
    if (type == ErrorType.ERROR_HTTPS_ONLY) {
        return context.getString(R.string.errorpage_httpsonly_title2)
    }
    // Returning `null` here will let the component use its default title for this error type
    return null
}

private fun getErrorPageDescription(context: Context, type: ErrorType): String? {
    if (type == ErrorType.ERROR_HTTPS_ONLY) {
        return context.getString(
            R.string.errorpage_httpsonly_message2,
            context.getString(R.string.app_name),
            SupportUtils.getGenericSumoURLForTopic(SupportUtils.SumoTopic.HTTPS_ONLY),
        )
    }
    // Returning `null` here will let the component use its default description for this error type
    return null
}
