/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.errorpages

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.errorpages.ErrorPages.createErrorPage
import mozilla.components.browser.errorpages.ErrorPages.createUrlEncodedErrorPage
import mozilla.components.support.ktx.kotlin.urlEncode
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.ArgumentMatchers.nullable
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class ErrorPagesTest {

    @Test
    fun `createErrorPage should replace default HTML placeholders`() {
        assertTemplateIsValid(ErrorType.UNKNOWN)
        assertTemplateIsValid(ErrorType.ERROR_SECURITY_SSL)
        assertTemplateIsValid(ErrorType.ERROR_SECURITY_BAD_CERT)
        assertTemplateIsValid(ErrorType.ERROR_NET_INTERRUPT)
        assertTemplateIsValid(ErrorType.ERROR_NET_TIMEOUT)
        assertTemplateIsValid(ErrorType.ERROR_CONNECTION_REFUSED)
        assertTemplateIsValid(ErrorType.ERROR_UNKNOWN_SOCKET_TYPE)
        assertTemplateIsValid(ErrorType.ERROR_REDIRECT_LOOP)
        assertTemplateIsValid(ErrorType.ERROR_OFFLINE)
        assertTemplateIsValid(ErrorType.ERROR_PORT_BLOCKED)
        assertTemplateIsValid(ErrorType.ERROR_NET_RESET)
        assertTemplateIsValid(ErrorType.ERROR_UNSAFE_CONTENT_TYPE)
        assertTemplateIsValid(ErrorType.ERROR_CORRUPTED_CONTENT)
        assertTemplateIsValid(ErrorType.ERROR_CONTENT_CRASHED)
        assertTemplateIsValid(ErrorType.ERROR_INVALID_CONTENT_ENCODING)
        assertTemplateIsValid(ErrorType.ERROR_UNKNOWN_HOST)
        assertTemplateIsValid(ErrorType.ERROR_MALFORMED_URI)
        assertTemplateIsValid(ErrorType.ERROR_UNKNOWN_PROTOCOL)
        assertTemplateIsValid(ErrorType.ERROR_FILE_NOT_FOUND)
        assertTemplateIsValid(ErrorType.ERROR_FILE_ACCESS_DENIED)
        assertTemplateIsValid(ErrorType.ERROR_PROXY_CONNECTION_REFUSED)
        assertTemplateIsValid(ErrorType.ERROR_UNKNOWN_PROXY_HOST)
        assertTemplateIsValid(ErrorType.ERROR_SAFEBROWSING_MALWARE_URI)
        assertTemplateIsValid(ErrorType.ERROR_SAFEBROWSING_UNWANTED_URI)
        assertTemplateIsValid(ErrorType.ERROR_SAFEBROWSING_HARMFUL_URI)
        assertTemplateIsValid(ErrorType.ERROR_SAFEBROWSING_PHISHING_URI)
    }

    @Test
    fun `createUrlEncodedErrorPage should encoded error information into the URL`() {
        assertUrlEncodingIsValid(ErrorType.UNKNOWN)
        assertUrlEncodingIsValid(ErrorType.ERROR_SECURITY_SSL)
        assertUrlEncodingIsValid(ErrorType.ERROR_SECURITY_BAD_CERT)
        assertUrlEncodingIsValid(ErrorType.ERROR_NET_INTERRUPT)
        assertUrlEncodingIsValid(ErrorType.ERROR_NET_TIMEOUT)
        assertUrlEncodingIsValid(ErrorType.ERROR_CONNECTION_REFUSED)
        assertUrlEncodingIsValid(ErrorType.ERROR_UNKNOWN_SOCKET_TYPE)
        assertUrlEncodingIsValid(ErrorType.ERROR_REDIRECT_LOOP)
        assertUrlEncodingIsValid(ErrorType.ERROR_OFFLINE)
        assertUrlEncodingIsValid(ErrorType.ERROR_PORT_BLOCKED)
        assertUrlEncodingIsValid(ErrorType.ERROR_NET_RESET)
        assertUrlEncodingIsValid(ErrorType.ERROR_UNSAFE_CONTENT_TYPE)
        assertUrlEncodingIsValid(ErrorType.ERROR_CORRUPTED_CONTENT)
        assertUrlEncodingIsValid(ErrorType.ERROR_CONTENT_CRASHED)
        assertUrlEncodingIsValid(ErrorType.ERROR_INVALID_CONTENT_ENCODING)
        assertUrlEncodingIsValid(ErrorType.ERROR_UNKNOWN_HOST)
        assertUrlEncodingIsValid(ErrorType.ERROR_MALFORMED_URI)
        assertUrlEncodingIsValid(ErrorType.ERROR_UNKNOWN_PROTOCOL)
        assertUrlEncodingIsValid(ErrorType.ERROR_FILE_NOT_FOUND)
        assertUrlEncodingIsValid(ErrorType.ERROR_FILE_ACCESS_DENIED)
        assertUrlEncodingIsValid(ErrorType.ERROR_PROXY_CONNECTION_REFUSED)
        assertUrlEncodingIsValid(ErrorType.ERROR_UNKNOWN_PROXY_HOST)
        assertUrlEncodingIsValid(ErrorType.ERROR_SAFEBROWSING_MALWARE_URI)
        assertUrlEncodingIsValid(ErrorType.ERROR_SAFEBROWSING_UNWANTED_URI)
        assertUrlEncodingIsValid(ErrorType.ERROR_SAFEBROWSING_HARMFUL_URI)
        assertUrlEncodingIsValid(ErrorType.ERROR_SAFEBROWSING_PHISHING_URI)
    }

    private fun assertTemplateIsValid(errorType: ErrorType) {
        val context = spy(testContext)
        val errorPage = createErrorPage(context, errorType)

        assertFalse(errorPage.contains("%pageTitle%"))
        assertFalse(errorPage.contains("%backButton%"))
        assertFalse(errorPage.contains("%button%"))
        assertFalse(errorPage.contains("%messageShort%"))
        assertFalse(errorPage.contains("%messageLong%"))
        assertFalse(errorPage.contains("%css%"))

        if (errorType == ErrorType.ERROR_SECURITY_SSL || errorType == ErrorType.ERROR_SECURITY_BAD_CERT) {
            assertFalse(errorPage.contains("%showSSL%"))
            assertFalse(errorPage.contains("%badCertAdvanced%"))
            assertFalse(errorPage.contains("%badCertTechInfo%"))
            assertFalse(errorPage.contains("%badCertGoBack%"))
            assertFalse(errorPage.contains("%badCertAcceptTemporary%"))
            verify(context, times(7)).getString(anyInt())
            verify(context, times(1)).getString(anyInt(), nullable(String::class.java))
            verify(context, times(1)).getString(anyInt(), nullable(String::class.java), nullable(String::class.java))
        } else {
            verify(context, times(4)).getString(anyInt())
            verify(context, times(1)).getString(anyInt(), nullable(String::class.java))
        }
    }

    private fun assertUrlEncodingIsValid(errorType: ErrorType) {
        val context = spy(testContext)

        val htmlFilename = "htmlResource.html"

        val uri = "sampleUri"

        val errorPage = createUrlEncodedErrorPage(
            context,
            errorType,
            uri,
            htmlFilename
        )

        val expectedImageName = if (errorType.imageNameRes != null) {
            context.resources.getString(errorType.imageNameRes!!) + ".svg"
        } else {
            ""
        }

        assertTrue(errorPage.startsWith("resource://android/assets/$htmlFilename"))
        assertTrue(errorPage.contains("&button=${context.resources.getString(errorType.refreshButtonRes).urlEncode()}"))

        val description = context.resources.getString(errorType.messageRes, uri).replace("<ul>", "<ul role=\"presentation\">")

        assertTrue(errorPage.contains("&description=${description.urlEncode()}"))
        assertTrue(errorPage.contains("&image=$expectedImageName"))
    }
}
