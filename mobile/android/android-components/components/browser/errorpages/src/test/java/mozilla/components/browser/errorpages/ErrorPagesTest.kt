/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.errorpages

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.errorpages.ErrorPages.createErrorPage
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.nullable
import org.mockito.Mockito.anyInt
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

    private fun assertTemplateIsValid(errorType: ErrorType) {
        val context = spy(testContext)

        val errorPage = createErrorPage(context, errorType)

        assertFalse(errorPage.contains("%pageTitle%"))
        assertFalse(errorPage.contains("%backButton%"))
        assertFalse(errorPage.contains("%button%"))
        assertFalse(errorPage.contains("%messageShort%"))
        assertFalse(errorPage.contains("%messageLong%"))
        assertFalse(errorPage.contains("%css%"))

        verify(context, times(4)).getString(anyInt())
        verify(context, times(1)).getString(anyInt(), nullable(String::class.java))
    }
}
