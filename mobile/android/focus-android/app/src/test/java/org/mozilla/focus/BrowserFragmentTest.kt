/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus

import android.content.Context
import android.graphics.Bitmap
import android.util.AttributeSet
import android.view.LayoutInflater
import android.view.View
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.selection.SelectionActionDelegate
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertNotNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions
import org.mozilla.focus.databinding.FragmentBrowserBinding
import org.mozilla.focus.widget.ResizableKeyboardCoordinatorLayout
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class BrowserFragmentTest {
    @Test
    fun testEngineViewInflationAndParentInteraction() {
        val layoutInflater = LayoutInflater.from(testContext)

        // Intercept the inflation process
        layoutInflater.factory2 = object : LayoutInflater.Factory2 {
            override fun onCreateView(
                parent: View?,
                name: String,
                context: Context,
                attrs: AttributeSet,
            ): View? {
                // Inflate a DummyEngineView when trying to inflate an EngineView
                if (name == EngineView::class.java.name) {
                    return DummyEngineView(testContext)
                }

                // For other types of views, let the system handle it
                return null
            }

            override fun onCreateView(name: String, context: Context, attrs: AttributeSet): View? {
                return onCreateView(null, name, context, attrs)
            }
        }

        val binding = FragmentBrowserBinding.inflate(LayoutInflater.from(testContext))
        val engineView: EngineView = binding.engineView

        assertNotNull(engineView)

        // Get the layout parent of the EngineView
        val engineViewParent = spy(
            (engineView as View).parent as ResizableKeyboardCoordinatorLayout,
        )

        assertNotNull(engineViewParent)

        engineViewParent.requestDisallowInterceptTouchEvent(true)

        // Verify that the EngineView's parent does not propagate requestDisallowInterceptTouchEvent
        verify(engineViewParent).requestDisallowInterceptTouchEvent(true)
        // If propagated, an additional ViewGroup.requestDisallowInterceptTouchEvent would have been registered.
        verifyNoMoreInteractions(engineViewParent)
    }
}

/**
 * Dummy implementation of the EngineView interface.
 */
class DummyEngineView(context: Context) : View(context), EngineView {
    init {
        id = R.id.engineView
    }

    override fun render(session: EngineSession) {
        // no-op
    }

    override fun release() {
        // no-op
    }

    override fun captureThumbnail(onFinish: (Bitmap?) -> Unit) {
        // no-op
    }

    override fun setVerticalClipping(clippingHeight: Int) {
        // no-op
    }

    override fun setDynamicToolbarMaxHeight(height: Int) {
        // no-op
    }

    override fun setActivityContext(context: Context?) {
        // no-op
    }

    override var selectionActionDelegate: SelectionActionDelegate? = null
}
