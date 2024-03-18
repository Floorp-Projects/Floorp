/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.findinpage

import mozilla.components.browser.state.state.SessionState
import mozilla.components.feature.findinpage.internal.FindInPageInteractor
import mozilla.components.feature.findinpage.internal.FindInPagePresenter
import mozilla.components.support.test.mock
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

class FindInPageFeatureTest {

    @Test
    fun `start is forwarded to presenter and interactor`() {
        val presenter: FindInPagePresenter = mock()
        val interactor: FindInPageInteractor = mock()

        val feature = FindInPageFeature(mock(), mock(), mock())
        feature.presenter = presenter
        feature.interactor = interactor

        feature.start()

        verify(presenter).start()
        verify(interactor).start()
    }

    @Test
    fun `stop is forwarded to presenter and interactor`() {
        val presenter: FindInPagePresenter = mock()
        val interactor: FindInPageInteractor = mock()

        val feature = FindInPageFeature(mock(), mock(), mock())
        feature.presenter = presenter
        feature.interactor = interactor

        feature.stop()

        verify(presenter).stop()
        verify(interactor).stop()
    }

    @Test
    fun `bind is forwarded to presenter and interactor`() {
        val presenter: FindInPagePresenter = mock()
        val interactor: FindInPageInteractor = mock()

        val feature = FindInPageFeature(mock(), mock(), mock())
        feature.presenter = presenter
        feature.interactor = interactor

        val session: SessionState = mock()
        feature.bind(session)

        verify(presenter).bind(session)
        verify(interactor).bind(session)
    }

    @Test
    fun `onBackPressed unbinds if bound to session`() {
        val presenter: FindInPagePresenter = mock()
        val interactor: FindInPageInteractor = mock()

        val feature = spy(FindInPageFeature(mock(), mock(), mock()))
        feature.presenter = presenter
        feature.interactor = interactor

        val session: SessionState = mock()
        feature.bind(session)

        assertTrue(feature.onBackPressed())

        verify(feature).unbind()
    }

    @Test
    fun `onBackPressed returns false if not bound`() {
        val presenter: FindInPagePresenter = mock()
        val interactor: FindInPageInteractor = mock()

        val feature = spy(FindInPageFeature(mock(), mock(), mock()))
        feature.presenter = presenter
        feature.interactor = interactor

        assertFalse(feature.onBackPressed())

        verify(feature, never()).unbind()
    }

    @Test
    fun `unbind is forwarded to presenter and interactor`() {
        val presenter: FindInPagePresenter = mock()
        val interactor: FindInPageInteractor = mock()

        val feature = FindInPageFeature(mock(), mock(), mock())
        feature.presenter = presenter
        feature.interactor = interactor

        feature.unbind()

        verify(presenter).unbind()
        verify(interactor).unbind()
    }

    @Test
    fun `unbind invokes close lambda`() {
        val presenter: FindInPagePresenter = mock()
        val interactor: FindInPageInteractor = mock()

        var lambdaInvoked = false

        val feature = FindInPageFeature(mock(), mock(), mock()) {
            lambdaInvoked = true
        }

        feature.presenter = presenter
        feature.interactor = interactor

        feature.unbind()

        assertTrue(lambdaInvoked)
    }
}
