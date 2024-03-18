/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.verify

class ToolbarFeatureTest {

    @Test
    fun `when app is backgrounded, toolbar onStop method is called`() {
        val toolbar: Toolbar = mock()
        val toolbarFeature = ToolbarFeature(toolbar, store = mock(), loadUrlUseCase = mock())

        toolbarFeature.stop()

        verify(toolbar).onStop()
    }

    @Test
    fun `GIVEN ToolbarFeature, WHEN start() is called THEN it should call controller#start()`() {
        val mockedController: ToolbarBehaviorController = mock()
        val feature = ToolbarFeature(mock(), mock(), mock()).apply {
            controller = mockedController
            // mock other dependencies to limit real code running and error-ing.
            presenter = mock()
            interactor = mock()
        }

        feature.start()

        verify(mockedController).start()
    }

    @Test
    fun `GIVEN ToolbarFeature, WHEN start() is called THEN it should call presenter#start()`() {
        val mockedPresenter: ToolbarPresenter = mock()
        val feature = ToolbarFeature(mock(), mock(), mock()).apply {
            controller = mock()
            presenter = mockedPresenter
            interactor = mock()
        }

        feature.start()

        verify(mockedPresenter).start()
    }

    @Test
    fun `GIVEN ToolbarFeature, WHEN start() is called THEN it should call interactor#start()`() {
        val mockedInteractor: ToolbarInteractor = mock()
        val feature = ToolbarFeature(mock(), mock(), mock()).apply {
            controller = mock()
            presenter = mock()
            interactor = mockedInteractor
        }

        feature.start()

        verify(mockedInteractor).start()
    }

    @Test
    fun `GIVEN ToolbarFeature, WHEN stop() is called THEN it should call controller#stop()`() {
        val mockedController: ToolbarBehaviorController = mock()
        val feature = ToolbarFeature(mock(), mock(), mock()).apply {
            controller = mockedController
        }

        feature.stop()

        verify(mockedController).stop()
    }

    @Test
    fun `GIVEN ToolbarFeature, WHEN stop() is called THEN it should call presenter#stop()`() {
        val mockedPresenter: ToolbarPresenter = mock()
        val feature = ToolbarFeature(mock(), mock(), mock()).apply {
            presenter = mockedPresenter
        }

        feature.stop()

        verify(mockedPresenter).stop()
    }

    @Test
    fun `GIVEN ToolbarFeature, WHEN onBackPressed() is called THEN it should call toolbar#onBackPressed()`() {
        val toolbar: Toolbar = mock()
        val feature = ToolbarFeature(toolbar, store = mock(), loadUrlUseCase = mock())

        feature.onBackPressed()

        verify(toolbar).onBackPressed()
    }
}
