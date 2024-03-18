/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.readerview.internal

import mozilla.components.feature.readerview.ReaderViewFeature
import mozilla.components.feature.readerview.view.ReaderViewControlsView
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Test
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions

class ReaderViewControlsInteractorTest {

    @Test
    fun `interactor assigns listener to self`() {
        val view = mock<ReaderViewControlsView>()
        val interactor = ReaderViewControlsInteractor(view, mock())

        interactor.start()

        verify(view).listener = interactor
        verifyNoMoreInteractions(view)
    }

    @Test
    fun `interactor un-assigns self from listener`() {
        val view = mock<ReaderViewControlsView>()
        val interactor = ReaderViewControlsInteractor(view, mock())

        interactor.stop()

        verify(view).listener = null
        verifyNoMoreInteractions(view)
    }

    @Test
    fun `update config on change`() {
        val config: ReaderViewConfig = mock()
        val interactor = ReaderViewControlsInteractor(mock(), config)

        interactor.onFontChanged(ReaderViewFeature.FontType.SANSSERIF)

        verify(config).fontType = ReaderViewFeature.FontType.SANSSERIF

        interactor.onColorSchemeChanged(ReaderViewFeature.ColorScheme.SEPIA)

        verify(config).colorScheme = ReaderViewFeature.ColorScheme.SEPIA
    }

    @Test
    fun `update config when font size increased`() {
        val config: ReaderViewConfig = mock()
        val interactor = ReaderViewControlsInteractor(mock(), config)

        whenever(config.fontSize).thenReturn(7)
        interactor.onFontSizeIncreased()
        verify(config).fontSize = 8

        whenever(config.fontSize).thenReturn(8)
        interactor.onFontSizeIncreased()
        verify(config).fontSize = 9

        // Can't increase past MAX_FONT_SIZE
        whenever(config.fontSize).thenReturn(9)
        interactor.onFontSizeIncreased()
        verify(config).fontSize = 9
    }

    @Test
    fun `update config when font size decreased`() {
        val config: ReaderViewConfig = mock()
        val interactor = ReaderViewControlsInteractor(mock(), config)

        whenever(config.fontSize).thenReturn(3)
        interactor.onFontSizeDecreased()
        verify(config).fontSize = 2

        whenever(config.fontSize).thenReturn(2)
        interactor.onFontSizeDecreased()
        verify(config).fontSize = 1

        // Can't decrease below MIN_FONT_SIZE
        whenever(config.fontSize).thenReturn(1)
        interactor.onFontSizeDecreased()
        verify(config).fontSize = 1
    }
}
