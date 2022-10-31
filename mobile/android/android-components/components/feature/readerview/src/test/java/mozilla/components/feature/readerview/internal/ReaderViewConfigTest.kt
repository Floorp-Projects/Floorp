/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.readerview.internal

import android.content.Context
import android.content.SharedPreferences
import android.content.res.Configuration
import android.content.res.Resources
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.feature.readerview.ReaderViewFeature
import mozilla.components.feature.readerview.ReaderViewFeature.Companion.COLOR_SCHEME_KEY
import mozilla.components.feature.readerview.ReaderViewFeature.Companion.FONT_SIZE_DEFAULT
import mozilla.components.feature.readerview.ReaderViewFeature.Companion.FONT_SIZE_KEY
import mozilla.components.feature.readerview.ReaderViewFeature.Companion.FONT_TYPE_KEY
import mozilla.components.support.test.eq
import mozilla.components.support.test.whenever
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.Mock
import org.mockito.Mockito.anyString
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.MockitoAnnotations

@RunWith(AndroidJUnit4::class)
class ReaderViewConfigTest {

    @Mock private lateinit var context: Context

    @Mock private lateinit var prefs: SharedPreferences

    @Mock private lateinit var editor: SharedPreferences.Editor

    @Mock private lateinit var sendConfigMessage: (JSONObject) -> Unit

    @Mock private lateinit var resources: Resources

    @Mock private lateinit var configuration: Configuration

    private lateinit var config: ReaderViewConfig

    @Before
    fun setup() {
        MockitoAnnotations.openMocks(this)
        whenever(context.getSharedPreferences(anyString(), anyInt())).thenReturn(prefs)
        whenever(prefs.edit()).thenReturn(editor)
        whenever(context.resources).thenReturn(resources)
        whenever(resources.configuration).thenReturn(configuration)
        configuration.uiMode = Configuration.UI_MODE_NIGHT_UNDEFINED

        config = ReaderViewConfig(context, sendConfigMessage)
    }

    @Test
    fun `color scheme should read from shared prefs`() {
        whenever(prefs.getString(COLOR_SCHEME_KEY, "LIGHT")).thenReturn("SEPIA")
        verify(prefs, never()).getString(eq(COLOR_SCHEME_KEY), anyString())

        assertEquals(ReaderViewFeature.ColorScheme.SEPIA, config.colorScheme)
        verify(prefs, times(1)).getString(eq(COLOR_SCHEME_KEY), anyString())

        assertEquals(ReaderViewFeature.ColorScheme.SEPIA, config.colorScheme)
        verify(prefs, times(1)).getString(eq(COLOR_SCHEME_KEY), anyString())
    }

    @Test
    fun `color scheme default should respect active dark mode`() {
        whenever(prefs.getString(COLOR_SCHEME_KEY, "LIGHT")).thenReturn("LIGHT")
        whenever(prefs.getString(COLOR_SCHEME_KEY, "DARK")).thenReturn("DARK")
        // reset config and test for a default of DARK for dark mode
        configuration.uiMode = Configuration.UI_MODE_NIGHT_YES
        config = ReaderViewConfig(context, sendConfigMessage)
        assertEquals(ReaderViewFeature.ColorScheme.DARK, config.colorScheme)
        // reset config and test for a default of LIGHT for explicit non-dark (light) mode
        configuration.uiMode = Configuration.UI_MODE_NIGHT_NO
        config = ReaderViewConfig(context, sendConfigMessage)
        assertEquals(ReaderViewFeature.ColorScheme.LIGHT, config.colorScheme)
        // test for UI_MODE_NIGHT_UNDEFINED was already done in the above test function
    }

    @Test
    fun `font type should read from shared prefs`() {
        whenever(prefs.getString(FONT_TYPE_KEY, "SERIF")).thenReturn("SANSSERIF")
        verify(prefs, never()).getString(eq(FONT_TYPE_KEY), anyString())

        assertEquals(ReaderViewFeature.FontType.SANSSERIF, config.fontType)
        verify(prefs, times(1)).getString(eq(FONT_TYPE_KEY), anyString())

        assertEquals(ReaderViewFeature.FontType.SANSSERIF, config.fontType)
        verify(prefs, times(1)).getString(eq(FONT_TYPE_KEY), anyString())
    }

    @Test
    fun `font size should read from shared prefs`() {
        whenever(prefs.getInt(FONT_SIZE_KEY, FONT_SIZE_DEFAULT)).thenReturn(4)
        verify(prefs, never()).getInt(eq(FONT_SIZE_KEY), anyInt())

        assertEquals(4, config.fontSize)
        verify(prefs, times(1)).getInt(eq(FONT_SIZE_KEY), anyInt())

        assertEquals(4, config.fontSize)
        verify(prefs, times(1)).getInt(eq(FONT_SIZE_KEY), anyInt())
    }
}
