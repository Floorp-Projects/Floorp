package mozilla.components.browser.errorpages

import android.content.Context
import android.support.annotation.RawRes
import android.support.annotation.StringRes

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
        UNKNOWN(
            R.string.error_generic_title,
            R.string.error_generic_message
        ),
        ERROR_SECURITY_SSL(
            R.string.error_security_ssl_title,
            R.string.error_security_ssl_message
        ),
        ERROR_SECURITY_BAD_CERT(
            R.string.error_security_bad_cert_title,
            R.string.error_security_bad_cert_message
        ),
        ERROR_NET_INTERRUPT(
            R.string.error_net_interrupt_title,
            R.string.error_net_interrupt_message
        ),
        ERROR_NET_TIMEOUT(
            R.string.error_net_timeout_title,
            R.string.error_net_timeout_message
        ),
        ERROR_CONNECTION_REFUSED(
            R.string.error_connection_failure_title,
            R.string.error_connection_failure_message
        ),
        ERROR_UNKNOWN_SOCKET_TYPE(
            R.string.error_unknown_socket_type_title,
            R.string.error_unknown_socket_type_message
        ),
        ERROR_REDIRECT_LOOP(
            R.string.error_redirect_loop_title,
            R.string.error_redirect_loop_message
        ),
        ERROR_OFFLINE(
            R.string.error_offline_title,
            R.string.error_offline_message
        ),
        ERROR_PORT_BLOCKED(
            R.string.error_port_blocked_title,
            R.string.error_port_blocked_message
        ),
        ERROR_NET_RESET(
            R.string.error_net_reset_title,
            R.string.error_net_reset_message
        ),
        ERROR_UNSAFE_CONTENT_TYPE(
            R.string.error_unsafe_content_type_title,
            R.string.error_unsafe_content_type_message
        ),
        ERROR_CORRUPTED_CONTENT(
            R.string.error_corrupted_content_title,
            R.string.error_corrupted_content_message
        ),
        ERROR_CONTENT_CRASHED(
            R.string.error_content_crashed_title,
            R.string.error_content_crashed_message
        ),
        ERROR_INVALID_CONTENT_ENCODING(
            R.string.error_invalid_content_encoding_title,
            R.string.error_invalid_content_encoding_message
        ),
        ERROR_UNKNOWN_HOST(
            R.string.error_unknown_host_title,
            R.string.error_unknown_host_message
        ),
        ERROR_MALFORMED_URI(
            R.string.error_malformed_uri_title,
            R.string.error_malformed_uri_message
        ),
        ERROR_UNKNOWN_PROTOCOL(
            R.string.error_unknown_protocol_title,
            R.string.error_unknown_protocol_message
        ),
        ERROR_FILE_NOT_FOUND(
            R.string.error_file_not_found_title,
            R.string.error_file_not_found_message
        ),
        ERROR_FILE_ACCESS_DENIED(
            R.string.error_file_access_denied_title,
            R.string.error_file_access_denied_message
        ),
        ERROR_PROXY_CONNECTION_REFUSED(
            R.string.error_proxy_connection_refused_title,
            R.string.error_proxy_connection_refused_message
        ),
        ERROR_UNKNOWN_PROXY_HOST(
            R.string.error_unknown_proxy_host_title,
            R.string.error_unknown_proxy_host_message
        ),
        ERROR_SAFEBROWSING_MALWARE_URI(
            R.string.error_safe_browsing_malware_uri_title,
            R.string.error_safe_browsing_malware_uri_message
        ),
        ERROR_SAFEBROWSING_UNWANTED_URI(
            R.string.error_safe_browsing_unwanted_uri_title,
            R.string.error_safe_browsing_unwanted_uri_message
        ),
        ERROR_SAFEBROWSING_HARMFUL_URI(
            R.string.error_safe_harmful_uri_title,
            R.string.error_safe_harmful_uri_message
        ),
        ERROR_SAFEBROWSING_PHISHING_URI(
            R.string.error_safe_phishing_uri_title,
            R.string.error_safe_phishing_uri_message
        )
    }

    /**
     * Load and generate error page for the given error type and html/css resources
     */
    fun createErrorPage(
        context: Context,
        errorType: ErrorType,
        @RawRes htmlRes: Int? = null,
        @RawRes cssRes: Int? = null
    ): String {
        @RawRes val htmlResource: Int = htmlRes ?: R.raw.error_pages
        @RawRes val cssResource: Int = cssRes ?: R.raw.error_style

        val css = context.resources.openRawResource(cssResource).bufferedReader().use {
            it.readText()
        }

        return context.resources.openRawResource(htmlResource)
            .bufferedReader()
            .use { it.readText() }
            .replace("%pageTitle%", context.getString(R.string.errorpage_title))
            .replace("%button%", context.getString(R.string.errorpage_refresh))
            .replace("%messageShort%", context.getString(errorType.titleRes))
            .replace("%messageLong%", context.getString(errorType.messageRes))
            .replace("%css%", css)
    }

    /**
     * Load and generate a private browsing page for the given url and html/css resources
     */
    fun createPrivateBrowsingPage(
        context: Context,
        url: String,
        @RawRes htmlRes: Int? = null,
        @RawRes cssRes: Int? = null
    ): String {
        @RawRes val htmlResource: Int = htmlRes ?: R.raw.private_mode
        @RawRes val cssResource: Int = cssRes ?: R.raw.private_style

        val css = context.resources.openRawResource(cssResource).bufferedReader().use {
            it.readText()
        }

        return context.resources.openRawResource(htmlResource)
            .bufferedReader()
            .use { it.readText() }
            .replace("%pageTitle%", context.getString(R.string.private_browsing_title))
            .replace("%pageBody%", context.getString(R.string.private_browsing_body))
            .replace("%privateBrowsingSupportUrl%", url)
            .replace("%css%", css)
    }
}
