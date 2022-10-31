/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine

import android.content.Context
import android.graphics.Bitmap
import android.widget.FrameLayout
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.selection.SelectionActionDelegate
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class EngineViewTest {

    @Test
    fun `asView method returns underlying Android view`() {
        val engineView = createDummyEngineView(testContext)

        val view = engineView.asView()

        assertTrue(view is FrameLayout)
    }

    @Test(expected = ClassCastException::class)
    fun `asView method fails if class is not a view`() {
        val engineView = BrokenEngineView()
        engineView.asView()
    }

    @Test
    fun lifecycleObserver() {
        val engineView = spy(createDummyEngineView(testContext))
        val observer = LifecycleObserver(engineView)

        observer.onCreate(mock())
        verify(engineView).onCreate()

        observer.onDestroy(mock())
        verify(engineView).onDestroy()

        observer.onStart(mock())
        verify(engineView).onStart()

        observer.onStop(mock())
        verify(engineView).onStop()

        observer.onPause(mock())
        verify(engineView).onPause()

        observer.onResume(mock())
        verify(engineView).onResume()
    }

    private fun createDummyEngineView(context: Context): EngineView = DummyEngineView(context)

    open class DummyEngineView(context: Context) : FrameLayout(context), EngineView {
        override fun setVerticalClipping(clippingHeight: Int) {}
        override fun setDynamicToolbarMaxHeight(height: Int) {}
        override fun captureThumbnail(onFinish: (Bitmap?) -> Unit) = Unit
        override fun render(session: EngineSession) {}
        override fun release() {}
        override var selectionActionDelegate: SelectionActionDelegate? = null
    }

    // Class it not actually a View!
    open class BrokenEngineView : EngineView {
        override fun setVerticalClipping(clippingHeight: Int) {}
        override fun setDynamicToolbarMaxHeight(height: Int) {}
        override fun captureThumbnail(onFinish: (Bitmap?) -> Unit) = Unit
        override fun render(session: EngineSession) {}
        override fun release() {}
        override var selectionActionDelegate: SelectionActionDelegate? = null
    }
}
