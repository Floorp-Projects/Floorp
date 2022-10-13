/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.feature

import android.app.Activity
import android.app.Activity.RESULT_OK
import android.content.Intent
import android.os.Looper.getMainLooper
import android.view.View
import android.view.WindowManager
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.LifecycleOwner
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.Robolectric
import org.robolectric.Shadows.shadowOf

@RunWith(AndroidJUnit4::class)
class ViewBoundFeatureWrapperTest {

    @Test
    fun `Calling onBackPressed on an empty wrapper returns false`() {
        val wrapper = ViewBoundFeatureWrapper<MockFeature>()
        assertFalse(wrapper.onBackPressed())
    }

    @Test
    fun `onBackPressed is forwarded to feature`() {
        val feature = MockFeatureWithUserInteractionHandler(onBackPressed = true)

        val wrapper = ViewBoundFeatureWrapper(
            feature = feature,
            owner = MockedLifecycleOwner(MockedLifecycle(Lifecycle.State.CREATED)),
            view = mock(),
        )

        assertTrue(wrapper.onBackPressed())
        assertTrue(feature.onBackPressedInvoked)

        assertFalse(
            ViewBoundFeatureWrapper(
                feature = MockFeatureWithUserInteractionHandler(onBackPressed = false),
                owner = MockedLifecycleOwner(MockedLifecycle(Lifecycle.State.CREATED)),
                view = mock(),
            ).onBackPressed(),
        )
    }

    @Test
    fun `Calling onActivityResult on an empty wrapper returns false`() {
        val wrapper = ViewBoundFeatureWrapper<MockFeature>()
        assertFalse(wrapper.onActivityResult(0, mock(), RESULT_OK))
    }

    @Test
    fun `onActivityResult is forwarded to feature`() {
        val feature = MockFeatureWithActivityResultHandler(onActivityResult = true)

        val wrapper = ViewBoundFeatureWrapper(
            feature = feature,
            owner = MockedLifecycleOwner(MockedLifecycle(Lifecycle.State.CREATED)),
            view = mock(),
        )

        assertTrue(wrapper.onActivityResult(1, null, RESULT_OK))
        assertTrue(feature.onActivityResultHandled)

        assertFalse(
            ViewBoundFeatureWrapper(
                feature = MockFeatureWithActivityResultHandler(onActivityResult = false),
                owner = MockedLifecycleOwner(MockedLifecycle(Lifecycle.State.CREATED)),
                view = mock(),
            ).onActivityResult(0, mock(), RESULT_OK),
        )
    }

    @Test
    fun `Setting feature registers lifecycle and view observers`() {
        val lifecycle = spy(MockedLifecycle(Lifecycle.State.CREATED))
        val owner = MockedLifecycleOwner(lifecycle)
        val view: View = mock()

        val feature = MockFeature()

        val wrapper = ViewBoundFeatureWrapper<MockFeature>()

        wrapper.set(
            feature = feature,
            owner = owner,
            view = view,
        )

        verify(lifecycle).addObserver(any())
        verify(view).addOnAttachStateChangeListener(any())
    }

    @Test
    fun `Lifecycle start event is forwarded to feature`() {
        val lifecycle = spy(MockedLifecycle(Lifecycle.State.CREATED))
        val owner = MockedLifecycleOwner(lifecycle)
        val view: View = mock()

        val feature = spy(MockFeature())

        val wrapper = ViewBoundFeatureWrapper<MockFeature>()

        wrapper.set(
            feature = feature,
            owner = owner,
            view = view,
        )

        verify(feature, never()).start()
        assertFalse(feature.started)

        lifecycle.observer!!.onStart(wrapper.owner!!)

        verify(feature).start()
        assertTrue(feature.started)
    }

    @Test
    fun `Lifecycle stop event is forwarded to feature`() {
        val lifecycle = spy(MockedLifecycle(Lifecycle.State.CREATED))
        val owner = MockedLifecycleOwner(lifecycle)
        val view: View = mock()

        val feature = spy(MockFeature())

        val wrapper = ViewBoundFeatureWrapper<MockFeature>()

        wrapper.set(
            feature = feature,
            owner = owner,
            view = view,
        )

        verify(feature, never()).stop()
        assertFalse(feature.started)

        lifecycle.observer!!.onStop(wrapper.owner!!)

        verify(feature).stop()
        assertFalse(feature.started)
    }

    @Test
    fun `Detaching View will call clear on wrapper`() {
        val activity = Robolectric.buildActivity(Activity::class.java).create().get()
        val view = View(activity)
        activity.windowManager.addView(view, WindowManager.LayoutParams(100, 100))
        shadowOf(getMainLooper()).idle()

        assertTrue(view.isAttachedToWindow)

        val wrapper = spy(ViewBoundFeatureWrapper<MockFeature>())

        wrapper.set(
            feature = MockFeature(),
            owner = MockedLifecycleOwner(MockedLifecycle(Lifecycle.State.CREATED)),
            view = view,
        )

        verify(wrapper, never()).clear()

        activity.windowManager.removeView(view)
        shadowOf(getMainLooper()).idle()

        verify(wrapper).clear()
    }

    @Test
    fun `Getter will return feature or null`() {
        val feature = MockFeature()

        val wrapper = ViewBoundFeatureWrapper<MockFeature>()

        assertNull(wrapper.get())

        wrapper.set(
            feature = feature,
            owner = MockedLifecycleOwner(MockedLifecycle(Lifecycle.State.CREATED)),
            view = mock(),
        )

        assertEquals(feature, wrapper.get())

        wrapper.clear()

        assertNull(wrapper.get())
    }

    @Test
    fun `WithFeature block is executed if feature reference is set`() {
        assertTrue(
            run {
                var blockExecuted = false

                val wrapper = ViewBoundFeatureWrapper<MockFeature>()
                wrapper.withFeature {
                    blockExecuted = true
                }

                assertFalse(blockExecuted)
                true
            },
        )

        assertTrue(
            run {
                var blockExecuted = false

                val wrapper = ViewBoundFeatureWrapper<MockFeature>()
                wrapper.set(
                    feature = MockFeature(),
                    owner = MockedLifecycleOwner(MockedLifecycle(Lifecycle.State.CREATED)),
                    view = mock(),
                )

                wrapper.withFeature {
                    blockExecuted = true
                }

                assertTrue(blockExecuted)
                true
            },
        )

        assertTrue(
            run {
                var blockExecuted = false

                val wrapper = ViewBoundFeatureWrapper<MockFeature>()
                wrapper.set(
                    feature = MockFeature(),
                    owner = MockedLifecycleOwner(MockedLifecycle(Lifecycle.State.CREATED)),
                    view = mock(),
                )

                wrapper.clear()

                wrapper.withFeature {
                    blockExecuted = true
                }

                assertFalse(blockExecuted)
                true
            },
        )
    }

    @Test
    fun `Clear removes observers`() {
        val lifecycle = spy(MockedLifecycle(Lifecycle.State.CREATED))
        val owner = MockedLifecycleOwner(lifecycle)
        val view: View = mock()

        val feature = MockFeature()

        val wrapper = ViewBoundFeatureWrapper<MockFeature>()

        wrapper.set(
            feature = feature,
            owner = owner,
            view = view,
        )

        verify(lifecycle, never()).removeObserver(any())
        verify(view, never()).removeOnAttachStateChangeListener(any())

        wrapper.clear()

        verify(lifecycle).removeObserver(any())
        verify(view).removeOnAttachStateChangeListener(any())
    }

    @Test
    fun `Clear stops started feature`() {
        val lifecycle = spy(MockedLifecycle(Lifecycle.State.CREATED))
        val owner = MockedLifecycleOwner(lifecycle)

        val feature = spy(MockFeature())

        val wrapper = ViewBoundFeatureWrapper<MockFeature>()

        wrapper.set(
            feature = feature,
            owner = owner,
            view = mock(),
        )

        lifecycle.observer!!.onStart(wrapper.owner!!)

        verify(feature, never()).stop()

        wrapper.clear()

        verify(feature).stop()
    }

    @Test
    fun `Clear does not stop already stopped feature`() {
        val lifecycle = spy(MockedLifecycle(Lifecycle.State.CREATED))
        val owner = MockedLifecycleOwner(lifecycle)

        val feature = spy(MockFeature())

        val wrapper = ViewBoundFeatureWrapper<MockFeature>()

        wrapper.set(
            feature = feature,
            owner = owner,
            view = mock(),
        )

        lifecycle.observer!!.onStart(wrapper.owner!!)
        lifecycle.observer!!.onStop(wrapper.owner!!)

        reset(feature)

        wrapper.clear()

        verify(feature, never()).stop()
    }

    @Test(expected = IllegalAccessError::class)
    fun `onBackPressed throws if feature does not implement BackHandler`() {
        val feature = MockFeature()

        val wrapper = ViewBoundFeatureWrapper(
            feature = feature,
            owner = MockedLifecycleOwner(MockedLifecycle(Lifecycle.State.CREATED)),
            view = mock(),
        )

        wrapper.onBackPressed()
    }

    @Test
    fun `Setting a feature clears a previously existing feature`() {
        val feature = MockFeature()

        val wrapper = spy(ViewBoundFeatureWrapper<MockFeature>())
        wrapper.set(
            feature = feature,
            owner = MockedLifecycleOwner(MockedLifecycle(Lifecycle.State.CREATED)),
            view = mock(),
        )

        assertEquals(feature, wrapper.get())
        verify(wrapper, never()).clear()

        val newFeature = MockFeature()
        wrapper.set(
            feature = newFeature,
            owner = MockedLifecycleOwner(MockedLifecycle(Lifecycle.State.CREATED)),
            view = mock(),
        )

        verify(wrapper).clear()
        assertEquals(newFeature, wrapper.get())
    }

    @Test
    fun `Clear can be called multiple times without side effects`() {
        val feature = MockFeature()

        val wrapper = spy(ViewBoundFeatureWrapper<MockFeature>())
        wrapper.set(
            feature = feature,
            owner = MockedLifecycleOwner(MockedLifecycle(Lifecycle.State.CREATED)),
            view = mock(),
        )

        assertEquals(feature, wrapper.get())

        wrapper.clear()
        wrapper.clear()
        wrapper.clear()

        assertNull(wrapper.get())
    }

    @Test
    fun `Lifecycle destroy event will clear wrapper`() {
        val lifecycle = spy(MockedLifecycle(Lifecycle.State.CREATED))
        val owner = MockedLifecycleOwner(lifecycle)
        val view: View = mock()

        val feature = spy(MockFeature())

        val wrapper = spy(ViewBoundFeatureWrapper<MockFeature>())

        wrapper.set(
            feature = feature,
            owner = owner,
            view = view,
        )

        verify(wrapper, never()).clear()

        lifecycle.observer!!.onDestroy(wrapper.owner!!)

        verify(wrapper).clear()
    }
}

private open class MockFeature : LifecycleAwareFeature {
    var started = false
        private set

    override fun start() {
        started = true
    }

    override fun stop() {
        started = false
    }
}

private class MockFeatureWithUserInteractionHandler(
    private val onBackPressed: Boolean = false,
) : MockFeature(), UserInteractionHandler {
    var onBackPressedInvoked = false
        private set

    override fun onBackPressed(): Boolean {
        onBackPressedInvoked = true
        return onBackPressed
    }
}

private class MockFeatureWithActivityResultHandler(
    private val onActivityResult: Boolean = false,
) : MockFeature(), ActivityResultHandler {
    var onActivityResultHandled = false
        private set

    override fun onActivityResult(requestCode: Int, data: Intent?, resultCode: Int): Boolean {
        onActivityResultHandled = true
        return onActivityResult
    }
}

private class MockedLifecycle(var state: State) : Lifecycle() {
    var observer: LifecycleBinding<*>? = null

    override fun addObserver(observer: LifecycleObserver) {
        this.observer = observer as LifecycleBinding<*>
    }

    override fun removeObserver(observer: LifecycleObserver) {
        this.observer = null
    }

    override fun getCurrentState(): State = state
}

private class MockedLifecycleOwner(private val lifecycle: MockedLifecycle) : LifecycleOwner {
    override fun getLifecycle(): Lifecycle = lifecycle
}
