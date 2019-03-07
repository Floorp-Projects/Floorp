/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.findinpage.internal

import android.content.Context
import android.view.View
import androidx.test.core.app.ApplicationProvider
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import mozilla.components.feature.findinpage.FindInPageFeature
import mozilla.components.feature.findinpage.view.FindInPageView
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class FindInPageInteractorTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    @Test
    fun `Start will register interactor as listener`() {
        val view: FindInPageView = mock()
        val interactor = FindInPageInteractor(mock(), mock(), view, mock())

        verify(view, never()).listener = interactor

        interactor.start()

        verify(view).listener = interactor
    }

    @Test
    fun `Stop will unregister interactor from listening to the view`() {
        val view: FindInPageView = mock()
        val interactor = FindInPageInteractor(mock(), mock(), view, mock())

        interactor.start()
        interactor.stop()

        verify(view).listener = null
    }

    @Test
    fun `FindInPageView-Listener implementation can get invoked without binding to session`() {
        val view: FindInPageView = mock()
        `when`(view.asView()).thenReturn(View(context))

        val interactor = FindInPageInteractor(mock(), mock(), view, mock())

        // Nothing should throw here if we haven't bound the interactor to a session
        interactor.onPreviousResult()
        interactor.onNextResult()
        interactor.onClose()
        interactor.onFindAll("example")
        interactor.onClearMatches()
    }

    @Test
    fun `OnPreviousResult updates engine session`() {
        val view: FindInPageView = mock()
        `when`(view.asView()).thenReturn(View(context))

        val engineSession: EngineSession = mock()

        val sessionManager: SessionManager = mock()
        `when`(sessionManager.getEngineSession(any())).thenReturn(engineSession)

        val interactor = FindInPageInteractor(mock(), sessionManager, view, mock())

        interactor.bind(mock())
        interactor.onPreviousResult()

        verify(engineSession).findNext(false)
    }

    @Test
    fun `onNextResult updates engine session`() {
        val view: FindInPageView = mock()
        `when`(view.asView()).thenReturn(View(context))

        val engineSession: EngineSession = mock()

        val sessionManager: SessionManager = mock()
        `when`(sessionManager.getEngineSession(any())).thenReturn(engineSession)

        val interactor = FindInPageInteractor(mock(), sessionManager, view, mock())

        interactor.bind(mock())
        interactor.onNextResult()

        verify(engineSession).findNext(true)
    }

    @Test
    fun `onNextResult blurs focused engine view`() {
        val view: FindInPageView = mock()
        `when`(view.asView()).thenReturn(View(context))

        val actualEngineView: View = mock()
        val engineView: EngineView = mock()
        `when`(engineView.asView()).thenReturn(actualEngineView)

        val interactor = FindInPageInteractor(mock(), mock(), view, engineView)

        interactor.bind(mock())
        interactor.onNextResult()
        verify(actualEngineView).clearFocus()
    }

    @Test
    fun `onClose notifies feature`() {
        val feature: FindInPageFeature = mock()

        val interactor = FindInPageInteractor(feature, mock(), mock(), mock())
        interactor.onClose()

        verify(feature).unbind()
    }

    @Test
    fun `unbind clears matches`() {
        val engineSession: EngineSession = mock()

        val sessionManager: SessionManager = mock()
        `when`(sessionManager.getEngineSession(any())).thenReturn(engineSession)

        val interactor = FindInPageInteractor(mock(), sessionManager, mock(), mock())

        interactor.bind(mock())
        interactor.unbind()

        verify(engineSession).clearFindMatches()
    }

    @Test
    fun `onFindAll updates engine session`() {
        val engineSession: EngineSession = mock()

        val sessionManager: SessionManager = mock()
        `when`(sessionManager.getEngineSession(any())).thenReturn(engineSession)

        val interactor = FindInPageInteractor(mock(), sessionManager, mock(), mock())

        interactor.bind(mock())
        interactor.onFindAll("example")

        verify(engineSession).findAll("example")
    }

    @Test
    fun `onClearMatches updates engine session`() {
        val engineSession: EngineSession = mock()

        val sessionManager: SessionManager = mock()
        `when`(sessionManager.getEngineSession(any())).thenReturn(engineSession)

        val interactor = FindInPageInteractor(mock(), sessionManager, mock(), mock())

        interactor.bind(mock())
        interactor.onClearMatches()

        verify(engineSession).clearFindMatches()
    }
}
