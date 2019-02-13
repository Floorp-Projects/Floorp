/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.icons.generator.IconGenerator
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class BrowserIconsTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    @Test
    fun `Uses generator`() {
        val mockedIcon: Icon = mock()

        val generator: IconGenerator = mock()
        `when`(generator.generate(any(), any())).thenReturn(mockedIcon)

        val request = IconRequest(url = "https://www.mozilla.org")
        val icon = BrowserIcons(context, generator = generator)
            .loadIcon(request)

        assertEquals(mockedIcon, runBlocking { icon.await() })

        verify(generator).generate(context, request)
    }
}
