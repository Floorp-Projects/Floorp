/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.readerview.internal

import android.view.View
import mozilla.components.feature.readerview.view.ReaderViewControlsView
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.verify

class ReaderViewControlsPresenterTest {

    @Test
    fun `start applies config to view`() {
        val config: ReaderViewConfig = mock()
        val view = mock<ReaderViewControlsView>()
        val presenter = ReaderViewControlsPresenter(view, config)

        whenever(config.colorScheme).thenReturn(mock())
        whenever(config.fontSize).thenReturn(5)
        whenever(config.fontType).thenReturn(mock())

        presenter.show()

        verify(view).tryInflate()
        verify(view).setColorScheme(any())
        verify(view).setFontSize(5)
        verify(view).setFont(any())
        verify(view).showControls()
    }

    @Test
    fun `are controls visible`() {
        val controlsView = mock<ReaderViewControlsView>()
        val view = mock<View>()
        whenever(controlsView.asView()).thenReturn(view)
        val presenter = ReaderViewControlsPresenter(controlsView, mock())

        whenever(view.visibility).thenReturn(View.GONE)
        assertFalse(presenter.areControlsVisible())

        whenever(view.visibility).thenReturn(View.VISIBLE)
        assertTrue(presenter.areControlsVisible())
    }

    @Test
    fun `hide updates the visibility of the controls`() {
        val view = mock<ReaderViewControlsView>()
        val presenter = ReaderViewControlsPresenter(view, mock())

        presenter.hide()

        verify(view).hideControls()
    }
}
