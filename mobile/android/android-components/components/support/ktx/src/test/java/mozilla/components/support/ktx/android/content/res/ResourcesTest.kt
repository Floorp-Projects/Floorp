/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.content.res

import android.content.res.Configuration
import android.content.res.Resources
import android.graphics.Typeface.BOLD
import android.graphics.Typeface.ITALIC
import android.os.Build
import android.os.LocaleList
import android.text.Html
import android.text.style.StyleSpan
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.robolectric.annotation.Config
import java.util.Locale

@RunWith(AndroidJUnit4::class)
class ResourcesTest {

    private lateinit var resources: Resources
    private lateinit var configuration: Configuration

    @Before
    fun setup() {
        resources = mock()
        configuration = spy(Configuration())

        whenever(resources.configuration).thenReturn(configuration)
    }

    @Config(sdk = [Build.VERSION_CODES.N])
    @Test
    fun `locale returns first item in locales list`() {
        whenever(configuration.locales).thenReturn(LocaleList(Locale.CANADA, Locale.ENGLISH))
        assertEquals(Locale.CANADA, resources.locale)
    }

    @Suppress("Deprecation")
    @Config(sdk = [Build.VERSION_CODES.M])
    @Test
    fun `locale returns locale from configuration`() {
        configuration.locale = Locale.FRENCH
        assertEquals(Locale.FRENCH, resources.locale)
    }

    @Config(sdk = [Build.VERSION_CODES.N])
    @Test
    fun `getSpanned formats corresponding string`() {
        val id = 100
        whenever(configuration.locales).thenReturn(LocaleList(Locale.ROOT))
        whenever(resources.getString(id)).thenReturn("Allow %1\$s to open %2\$s")

        assertEquals(
            "<p dir=\"ltr\">Allow <b>App</b> to open <i>Website</i></p>\n",
            Html.toHtml(
                resources.getSpanned(
                    id,
                    "App" to StyleSpan(BOLD),
                    "Website" to StyleSpan(ITALIC),
                ),
                Html.TO_HTML_PARAGRAPH_LINES_CONSECUTIVE,
            ),
        )
    }
}
