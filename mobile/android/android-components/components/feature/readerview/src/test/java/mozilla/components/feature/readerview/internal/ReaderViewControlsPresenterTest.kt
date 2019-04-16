/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.readerview.internal

import mozilla.components.feature.readerview.ReaderViewFeature
import mozilla.components.feature.readerview.view.ReaderViewControlsView
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.verify

class ReaderViewControlsPresenterTest {
    @Test
    fun `start applies config to view`() {
        val config: ReaderViewFeature.Config = mock()
        val view: ReaderViewControlsView = mock()
        val presenter = ReaderViewControlsPresenter(view, config)

        `when`(config.colorScheme).thenReturn(mock())
        `when`(config.fontSize).thenReturn(5)
        `when`(config.fontType).thenReturn(mock())

        presenter.show()

        verify(view).setColorScheme(any())
        verify(view).setFontSize(5)
        verify(view).setFont(any())
        verify(view).showControls()
    }

    @Test
    fun `hide updates the visibility of the controls`() {
        val view: ReaderViewControlsView = mock()
        val presenter = ReaderViewControlsPresenter(view, mock())

        presenter.hide()

        verify(view).hideControls()
    }
}
