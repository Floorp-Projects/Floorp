/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.errorpages

import android.content.Context
import androidx.annotation.RawRes
import androidx.annotation.StringRes

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

object ErrorPages {

    /**
     * Load and generate error page for the given error type and html/css resources
     */
    fun createErrorPage(
        context: Context,
        errorType: ErrorType,
        uri: String? = null,
        @RawRes htmlResource: Int = R.raw.error_pages,
        @RawRes cssResource: Int = R.raw.error_style
    ): String {
        val css = context.resources.openRawResource(cssResource).bufferedReader().use {
            it.readText()
        }

        return context.resources.openRawResource(htmlResource)
            .bufferedReader()
            .use { it.readText() }
            .replace("%pageTitle%", context.getString(R.string.mozac_browser_errorpages_page_title))
            .replace("%backButton%", context.getString(R.string.mozac_browser_errorpages_page_go_back))
            .replace("%button%", context.getString(R.string.mozac_browser_errorpages_page_refresh))
            .replace("%messageShort%", context.getString(errorType.titleRes))
            .replace("%messageLong%", context.getString(errorType.messageRes, uri))
            .replace("%css%", css)
    }
}

/**
 * Enum containing all supported error types that we can display an error page for.
 */
enum class ErrorType(
    @StringRes val titleRes: Int,
    @StringRes val messageRes: Int
) {
    UNKNOWN(
        R.string.mozac_browser_errorpages_generic_title,
        R.string.mozac_browser_errorpages_generic_message
    ),
    ERROR_SECURITY_SSL(
        R.string.mozac_browser_errorpages_security_ssl_title,
        R.string.mozac_browser_errorpages_security_ssl_message
    ),
    ERROR_SECURITY_BAD_CERT(
        R.string.mozac_browser_errorpages_security_bad_cert_title,
        R.string.mozac_browser_errorpages_security_bad_cert_message
    ),
    ERROR_NET_INTERRUPT(
        R.string.mozac_browser_errorpages_net_interrupt_title,
        R.string.mozac_browser_errorpages_net_interrupt_message
    ),
    ERROR_NET_TIMEOUT(
        R.string.mozac_browser_errorpages_net_timeout_title,
        R.string.mozac_browser_errorpages_net_timeout_message
    ),
    ERROR_CONNECTION_REFUSED(
        R.string.mozac_browser_errorpages_connection_failure_title,
        R.string.mozac_browser_errorpages_connection_failure_message
    ),
    ERROR_UNKNOWN_SOCKET_TYPE(
        R.string.mozac_browser_errorpages_unknown_socket_type_title,
        R.string.mozac_browser_errorpages_unknown_socket_type_message
    ),
    ERROR_REDIRECT_LOOP(
        R.string.mozac_browser_errorpages_redirect_loop_title,
        R.string.mozac_browser_errorpages_redirect_loop_message
    ),
    ERROR_OFFLINE(
        R.string.mozac_browser_errorpages_offline_title,
        R.string.mozac_browser_errorpages_offline_message
    ),
    ERROR_PORT_BLOCKED(
        R.string.mozac_browser_errorpages_port_blocked_title,
        R.string.mozac_browser_errorpages_port_blocked_message
    ),
    ERROR_NET_RESET(
        R.string.mozac_browser_errorpages_net_reset_title,
        R.string.mozac_browser_errorpages_net_reset_message
    ),
    ERROR_UNSAFE_CONTENT_TYPE(
        R.string.mozac_browser_errorpages_unsafe_content_type_title,
        R.string.mozac_browser_errorpages_unsafe_content_type_message
    ),
    ERROR_CORRUPTED_CONTENT(
        R.string.mozac_browser_errorpages_corrupted_content_title,
        R.string.mozac_browser_errorpages_corrupted_content_message
    ),
    ERROR_CONTENT_CRASHED(
        R.string.mozac_browser_errorpages_content_crashed_title,
        R.string.mozac_browser_errorpages_content_crashed_message
    ),
    ERROR_INVALID_CONTENT_ENCODING(
        R.string.mozac_browser_errorpages_invalid_content_encoding_title,
        R.string.mozac_browser_errorpages_invalid_content_encoding_message
    ),
    ERROR_UNKNOWN_HOST(
        R.string.mozac_browser_errorpages_unknown_host_title,
        R.string.mozac_browser_errorpages_unknown_host_message
    ),
    ERROR_MALFORMED_URI(
        R.string.mozac_browser_errorpages_malformed_uri_title,
        R.string.mozac_browser_errorpages_malformed_uri_message
    ),
    ERROR_UNKNOWN_PROTOCOL(
        R.string.mozac_browser_errorpages_unknown_protocol_title,
        R.string.mozac_browser_errorpages_unknown_protocol_message
    ),
    ERROR_FILE_NOT_FOUND(
        R.string.mozac_browser_errorpages_file_not_found_title,
        R.string.mozac_browser_errorpages_file_not_found_message
    ),
    ERROR_FILE_ACCESS_DENIED(
        R.string.mozac_browser_errorpages_file_access_denied_title,
        R.string.mozac_browser_errorpages_file_access_denied_message
    ),
    ERROR_PROXY_CONNECTION_REFUSED(
        R.string.mozac_browser_errorpages_proxy_connection_refused_title,
        R.string.mozac_browser_errorpages_proxy_connection_refused_message
    ),
    ERROR_UNKNOWN_PROXY_HOST(
        R.string.mozac_browser_errorpages_unknown_proxy_host_title,
        R.string.mozac_browser_errorpages_unknown_proxy_host_message
    ),
    ERROR_SAFEBROWSING_MALWARE_URI(
        R.string.mozac_browser_errorpages_safe_browsing_malware_uri_title,
        R.string.mozac_browser_errorpages_safe_browsing_malware_uri_message
    ),
    ERROR_SAFEBROWSING_UNWANTED_URI(
        R.string.mozac_browser_errorpages_safe_browsing_unwanted_uri_title,
        R.string.mozac_browser_errorpages_safe_browsing_unwanted_uri_message
    ),
    ERROR_SAFEBROWSING_HARMFUL_URI(
        R.string.mozac_browser_errorpages_safe_harmful_uri_title,
        R.string.mozac_browser_errorpages_safe_harmful_uri_message
    ),
    ERROR_SAFEBROWSING_PHISHING_URI(
        R.string.mozac_browser_errorpages_safe_phishing_uri_title,
        R.string.mozac_browser_errorpages_safe_phishing_uri_message
    )
}
