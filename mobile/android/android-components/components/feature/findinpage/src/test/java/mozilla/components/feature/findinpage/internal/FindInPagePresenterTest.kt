/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.findinpage.internal

import android.view.View
import mozilla.components.browser.session.Session
import mozilla.components.feature.findinpage.view.FindInPageView
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

class FindInPagePresenterTest {

    @Test
    fun `bind registers on session with view scope`() {
        val actualView: View = mock()

        val view: FindInPageView = mock()
        `when`(view.asView()).thenReturn(actualView)

        val session: Session = mock()

        val presenter = FindInPagePresenter(view)
        presenter.start()
        presenter.bind(session)

        verify(session).register(presenter, actualView)
    }

    @Test
    fun `bind does not register if presenter is not started`() {
        val actualView: View = mock()

        val view: FindInPageView = mock()
        `when`(view.asView()).thenReturn(actualView)

        val session: Session = mock()

        val presenter = FindInPagePresenter(view)
        presenter.bind(session)

        verify(session, never()).register(presenter, actualView)
    }

    @Test
    fun `Start will register on previously bound session`() {
        val actualView: View = mock()

        val view: FindInPageView = mock()
        `when`(view.asView()).thenReturn(actualView)

        val session: Session = mock()

        val presenter = FindInPagePresenter(view)
        presenter.bind(session)

        verify(session, never()).register(presenter, actualView)

        presenter.start()

        verify(session).register(presenter, actualView)
    }

    @Test
    fun `start re-registers on session with view scope`() {
        val actualView: View = mock()

        val view: FindInPageView = mock()
        `when`(view.asView()).thenReturn(actualView)

        val session: Session = mock()

        val presenter = FindInPagePresenter(view)
        presenter.bind(session)

        presenter.start()

        verify(session, times(1)).register(presenter, actualView)

        presenter.stop()

        verify(session, times(1)).register(presenter, actualView)

        presenter.start() // Should register again

        verify(session, times(2)).register(presenter, actualView)
    }

    @Test
    fun `stop unregisters session`() {
        val actualView: View = mock()

        val view: FindInPageView = mock()
        `when`(view.asView()).thenReturn(actualView)

        val session: Session = mock()

        val presenter = FindInPagePresenter(view)
        presenter.start()
        presenter.bind(session)

        verify(session).register(presenter, actualView)
        verify(session, never()).unregister(presenter)

        presenter.stop()

        verify(session).unregister(presenter)
    }

    @Test
    fun `bind will focus view`() {
        val view: FindInPageView = mock()

        val presenter = FindInPagePresenter(view)
        presenter.bind(mock())

        verify(view).focus()
    }

    @Test
    fun `Session-Observer-onFindResult will be forwarded to view`() {
        val view: FindInPageView = mock()

        val session: Session = mock()
        val result: Session.FindResult = mock()

        val presenter = FindInPagePresenter(view)
        presenter.onFindResult(session, result)

        verify(view).displayResult(result)
    }

    @Test
    fun `unbind will clear view`() {
        val view: FindInPageView = mock()

        val presenter = FindInPagePresenter(view)
        presenter.unbind()

        verify(view).clear()
    }

    @Test
    fun `unbind unregisters from session`() {
        val actualView: View = mock()

        val view: FindInPageView = mock()
        `when`(view.asView()).thenReturn(actualView)
        val session: Session = mock()

        val presenter = FindInPagePresenter(view)
        presenter.start()
        presenter.bind(session)

        verify(session).register(presenter, actualView)
        verify(session, never()).unregister(presenter)

        presenter.unbind()

        verify(session).unregister(presenter)
    }
}