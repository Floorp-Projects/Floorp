/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.readerview.internal

import mozilla.components.feature.readerview.ReaderViewFeature
import mozilla.components.feature.readerview.view.ReaderViewControlsView
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.reset
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions

class ReaderViewControlsInteractorTest {
    @Test
    fun `interactor assigns listener to self`() {
        val view: ReaderViewControlsView = mock()
        val interactor = ReaderViewControlsInteractor(view, mock())

        interactor.start()

        verify(view).listener = interactor
        verifyNoMoreInteractions(view)
    }

    @Test
    fun `interactor un-assigns self from listener`() {
        val view: ReaderViewControlsView = mock()
        val interactor = ReaderViewControlsInteractor(view, mock())

        interactor.stop()

        verify(view).listener = null
        verifyNoMoreInteractions(view)
    }

    @Test
    fun `update config on change`() {
        val config: ReaderViewFeature.Config = mock()
        val interactor = ReaderViewControlsInteractor(mock(), config)

        interactor.onFontChanged(ReaderViewFeature.Config.FontType.SANS_SERIF)

        verify(config).fontType = ReaderViewFeature.Config.FontType.SANS_SERIF

        interactor.onColorSchemeChanged(ReaderViewFeature.Config.ColorScheme.SEPIA)

        verify(config).colorScheme = ReaderViewFeature.Config.ColorScheme.SEPIA
    }

    @Test
    fun `update config font size on change`() {
        val config: ReaderViewFeature.Config = mock()
        val interactor = ReaderViewControlsInteractor(mock(), config)

        `when`(config.fontSize).thenReturn(5)
        interactor.onFontSizeIncreased()

        verify(config).fontSize += 1

        reset(config)

        interactor.onFontSizeDecreased()

        verify(config).fontSize -= 1
    }
}
