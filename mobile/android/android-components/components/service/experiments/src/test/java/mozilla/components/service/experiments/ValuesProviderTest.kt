/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy
import java.util.Locale
import java.util.MissingResourceException

@RunWith(AndroidJUnit4::class)
class ValuesProviderTest {
    @Test
    fun `get language has three letter code`() {
        Locale.setDefault(Locale("en", "US"))
        assertEquals("eng", ValuesProvider().getLanguage(mock(Context::class.java)))
    }

    @Test
    fun `get language doesn't have three letter code`() {
        val locale = spy(Locale.getDefault())
        `when`(locale.isO3Language).thenThrow(MissingResourceException("", "", ""))
        `when`(locale.language).thenReturn("language")
        Locale.setDefault(locale)
        assertEquals("language", ValuesProvider().getLanguage(mock(Context::class.java)))
    }

    @Test
    fun `get country has three letter code`() {
        Locale.setDefault(Locale("en", "US"))
        assertEquals("USA", ValuesProvider().getCountry(mock(Context::class.java)))
    }

    @Test
    fun `get country doesn't have three letter code`() {
        Locale.setDefault(Locale("cnr", "CS"))
        assertEquals("CS", ValuesProvider().getCountry(mock(Context::class.java)))
    }
}
