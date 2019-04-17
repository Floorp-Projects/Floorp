/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.browser.errorpages

import org.junit.Assert
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.nullable
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class ErrorPagesTest {

    @Test
    fun `createErrorPage should replace default HTML placeholders`() {
        val context = spy(RuntimeEnvironment.application)
        val errorPage = ErrorPages.createErrorPage(context, ErrorType.UNKNOWN)
        Assert.assertFalse(errorPage.contains("%pageTitle%"))
        Assert.assertFalse(errorPage.contains("%backButton%"))
        Assert.assertFalse(errorPage.contains("%button%"))
        Assert.assertFalse(errorPage.contains("%messageShort%"))
        Assert.assertFalse(errorPage.contains("%messageLong%"))
        Assert.assertFalse(errorPage.contains("%css%"))
        verify(context, times(4)).getString(anyInt())
        verify(context, times(1)).getString(anyInt(), nullable(String::class.java))
    }
}