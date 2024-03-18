/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.readerview.view

import android.view.View
import androidx.appcompat.widget.AppCompatButton
import androidx.appcompat.widget.AppCompatRadioButton
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.feature.readerview.R
import mozilla.components.feature.readerview.ReaderViewFeature
import mozilla.components.support.test.mock
import mozilla.ext.appCompatContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class ReaderViewControlsBarTest {

    @Test
    fun `flags are set on UI init`() {
        val bar = spy(ReaderViewControlsBar(appCompatContext))

        assertTrue(bar.isFocusableInTouchMode)
        assertTrue(bar.isClickable)
    }

    @Test
    fun `font options are set`() {
        val bar = ReaderViewControlsBar(appCompatContext)
        bar.tryInflate()

        val serifButton = bar.findViewById<AppCompatRadioButton>(R.id.mozac_feature_readerview_font_serif)
        val sansSerifButton = bar.findViewById<AppCompatRadioButton>(R.id.mozac_feature_readerview_font_sans_serif)

        assertFalse(serifButton.isChecked)

        bar.setFont(ReaderViewFeature.FontType.SERIF)

        assertTrue(serifButton.isChecked)

        assertFalse(sansSerifButton.isChecked)

        bar.setFont(ReaderViewFeature.FontType.SANSSERIF)

        assertTrue(sansSerifButton.isChecked)
    }

    @Test
    fun `font size buttons are enabled or disabled`() {
        val bar = ReaderViewControlsBar(appCompatContext)
        bar.tryInflate()

        val sizeDecreaseButton = bar.findViewById<AppCompatButton>(R.id.mozac_feature_readerview_font_size_decrease)
        val sizeIncreaseButton = bar.findViewById<AppCompatButton>(R.id.mozac_feature_readerview_font_size_increase)

        bar.setFontSize(5)

        assertTrue(sizeDecreaseButton.isEnabled)
        assertTrue(sizeIncreaseButton.isEnabled)

        bar.setFontSize(1)

        assertFalse(sizeDecreaseButton.isEnabled)
        assertTrue(sizeIncreaseButton.isEnabled)

        bar.setFontSize(0)

        assertFalse(sizeDecreaseButton.isEnabled)
        assertTrue(sizeIncreaseButton.isEnabled)

        bar.setFontSize(9)

        assertTrue(sizeDecreaseButton.isEnabled)
        assertFalse(sizeIncreaseButton.isEnabled)

        bar.setFontSize(10)

        assertTrue(sizeDecreaseButton.isEnabled)
        assertFalse(sizeIncreaseButton.isEnabled)
    }

    @Test
    fun `color scheme is set`() {
        val bar = ReaderViewControlsBar(appCompatContext)
        bar.tryInflate()

        val colorOptionDark = bar.findViewById<AppCompatRadioButton>(R.id.mozac_feature_readerview_color_dark)
        val colorOptionSepia = bar.findViewById<AppCompatRadioButton>(R.id.mozac_feature_readerview_color_sepia)
        val colorOptionLight = bar.findViewById<AppCompatRadioButton>(R.id.mozac_feature_readerview_color_light)

        bar.setColorScheme(ReaderViewFeature.ColorScheme.DARK)

        assertTrue(colorOptionDark.isChecked)
        assertFalse(colorOptionSepia.isChecked)
        assertFalse(colorOptionLight.isChecked)

        bar.setColorScheme(ReaderViewFeature.ColorScheme.SEPIA)

        assertFalse(colorOptionDark.isChecked)
        assertTrue(colorOptionSepia.isChecked)
        assertFalse(colorOptionLight.isChecked)

        bar.setColorScheme(ReaderViewFeature.ColorScheme.LIGHT)

        assertFalse(colorOptionDark.isChecked)
        assertFalse(colorOptionSepia.isChecked)
        assertTrue(colorOptionLight.isChecked)
    }

    @Test
    fun `showControls updates visibility and requests focus`() {
        val bar = spy(ReaderViewControlsBar(appCompatContext))

        bar.showControls()

        verify(bar).visibility = View.VISIBLE
        verify(bar).requestFocus()
    }

    @Test
    fun `hideControls updates visibility`() {
        val bar = spy(ReaderViewControlsBar(appCompatContext))

        bar.hideControls()

        verify(bar).visibility = View.GONE
    }

    @Test
    fun `when focus is lost, hide controls`() {
        val bar = spy(ReaderViewControlsBar(appCompatContext))

        bar.clearFocus()

        verify(bar, never()).hideControls()

        bar.requestFocus()

        bar.clearFocus()

        verify(bar).hideControls()
    }

    @Test
    fun `listener is invoked when clicking a font option`() {
        val bar = ReaderViewControlsBar(appCompatContext)
        val listener: ReaderViewControlsView.Listener = mock()

        assertNull(bar.listener)

        bar.listener = listener
        bar.tryInflate()

        bar.findViewById<AppCompatRadioButton>(R.id.mozac_feature_readerview_font_sans_serif).performClick()

        verify(listener).onFontChanged(ReaderViewFeature.FontType.SANSSERIF)
    }

    @Test
    fun `listener is invoked when clicking a font size option`() {
        val bar = ReaderViewControlsBar(appCompatContext)
        val listener: ReaderViewControlsView.Listener = mock()

        assertNull(bar.listener)

        bar.listener = listener
        bar.tryInflate()

        bar.findViewById<AppCompatButton>(R.id.mozac_feature_readerview_font_size_increase).performClick()

        verify(listener).onFontSizeIncreased()
    }

    @Test
    fun `listener is invoked when clicking a color scheme`() {
        val bar = ReaderViewControlsBar(appCompatContext)
        val listener: ReaderViewControlsView.Listener = mock()

        assertNull(bar.listener)

        bar.listener = listener
        bar.tryInflate()

        bar.findViewById<AppCompatRadioButton>(R.id.mozac_feature_readerview_color_sepia).performClick()

        verify(listener).onColorSchemeChanged(ReaderViewFeature.ColorScheme.SEPIA)
    }

    @Test
    fun `tryInflate is only successfully once`() {
        val bar = ReaderViewControlsBar(appCompatContext)

        assertTrue(bar.tryInflate())
        assertFalse(bar.tryInflate())
    }
}
