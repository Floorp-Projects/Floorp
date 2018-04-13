package mozilla.components.browser.errorpages

import android.content.Context
import android.support.annotation.StringRes
import android.webkit.WebViewClient

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

object ErrorPages {
    /**
     * Enum containing all supported error types that we can display an error page for.
     */
    enum class ErrorType(
        @StringRes val titleRes: Int,
        @StringRes val messageRes: Int
    ) {
        GENERIC(
                R.string.error_generic_title,
                R.string.error_generic_message),
        CONNECTION_FAILURE(
                R.string.error_connectionfailure_title,
                R.string.error_connectionfailure_message),
        HOST_LOOKUP(
                R.string.error_hostLookup_title,
                R.string.error_hostLookup_message),
        CONNECT(
                R.string.error_connect_title,
                R.string.error_connect_message),
        TIMEOUT(
                R.string.error_timeout_title,
                R.string.error_timeout_message),
        REDIRECT_LOOP (
                R.string.error_redirectLoop_title,
                R.string.error_redirectLoop_message),
        UNSUPPORTED_PROTOCOL(
                R.string.error_unsupportedprotocol_title,
                R.string.error_unsupportedprotocol_message),
        MALFORMED_URI(
                R.string.error_malformedURI_title,
                R.string.error_malformedURI_message),
        FAILED_SSL_HANDSHAKE(
                R.string.error_sslhandshake_title,
                R.string.error_sslhandshake_message)
    }

    /**
     * Map a WebView error code (WebViewClient.ERROR_*) to an element of the ErrorType enum.
     */
    fun mapWebViewErrorCodeToErrorType(errorCode: Int): ErrorType =
        // Chromium's mapping (internal error code, to Android WebView error code) is described at:
        // https://goo.gl/vspwct (ErrorCodeConversionHelper.java)
        when (errorCode) {
            WebViewClient.ERROR_UNKNOWN -> ErrorType.CONNECTION_FAILURE

            // This is probably the most commonly shown error. If there's no network, we inevitably
            // show this.
            WebViewClient.ERROR_HOST_LOOKUP -> ErrorType.HOST_LOOKUP

            WebViewClient.ERROR_CONNECT -> ErrorType.CONNECT

            // It's unclear what this actually means - it's not well documented. Based on looking at
            // ErrorCodeConversionHelper this could happen if networking is disabled during load, in which
            // case the generic error is good enough:
            WebViewClient.ERROR_IO -> ErrorType.CONNECTION_FAILURE

            WebViewClient.ERROR_TIMEOUT -> ErrorType.TIMEOUT

            WebViewClient.ERROR_REDIRECT_LOOP -> ErrorType.REDIRECT_LOOP

            WebViewClient.ERROR_UNSUPPORTED_SCHEME -> ErrorType.UNSUPPORTED_PROTOCOL

            WebViewClient.ERROR_FAILED_SSL_HANDSHAKE -> ErrorType.FAILED_SSL_HANDSHAKE

            WebViewClient.ERROR_BAD_URL -> ErrorType.MALFORMED_URI

            // Seems to be an indication of OOM, insufficient resources, or too many queued DNS queries
            WebViewClient.ERROR_TOO_MANY_REQUESTS -> ErrorType.GENERIC

            // There's no mapping for the following errors yet. At the time this library was
            // extracted from Focus we didn't use any of those errors.
            // WebViewClient.ERROR_UNSUPPORTED_AUTH_SCHEME
            // WebViewClient.ERROR_AUTHENTICATION
            // WebViewClient.ERROR_FILE
            // WebViewClient.ERROR_FILE_NOT_FOUND

            else -> throw IllegalArgumentException("No error type definition for error code $errorCode")
    }

    /**
     * Load and generate error page for the given error type.
     */
    fun createErrorPage(context: Context, errorType: ErrorType): String {
        val css = context.resources.openRawResource(R.raw.errorpage_style).bufferedReader().use {
            it.readText()
        }

        return context.resources.openRawResource(R.raw.errorpage)
                .bufferedReader()
                .use { it.readText() }
                .replace("%page-title%", context.getString(R.string.errorpage_title))
                .replace("%button%", context.getString(R.string.errorpage_refresh))
                .replace("%messageShort%", context.getString(errorType.titleRes))
                .replace("%messageLong%", context.getString(errorType.messageRes))
                .replace("%css%", css)
    }
}
