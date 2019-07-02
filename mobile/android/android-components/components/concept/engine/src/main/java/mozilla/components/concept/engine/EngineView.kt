/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine

import android.graphics.Bitmap
import android.view.View
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.OnLifecycleEvent

/**
 * View component that renders web content.
 */
@Suppress("TooManyFunctions")
interface EngineView {

    /**
     * Convenience method to cast the implementation of this interface to an Android View object.
     */
    fun asView(): View = this as View

    /**
     * Render the content of the given session.
     */
    fun render(session: EngineSession)

    /**
     * Releases an [EngineSession] that is currently rendered by this view (after calling [render]).
     *
     * Usually an app does not need to call this itself since [EngineView] will take care of that if it gets detached.
     * However there are situations where an app wants to hand-off rendering of an [EngineSession] to a different
     * [EngineView] without the current [EngineView] getting detached immediately.
     */
    fun release()

    /**
     * To be called in response to [Lifecycle.Event.ON_RESUME]. See [EngineView]
     * implementations for details.
     */
    fun onResume() = Unit

    /**
     * To be called in response to [Lifecycle.Event.ON_PAUSE]. See [EngineView]
     * implementations for details.
     */
    fun onPause() = Unit

    /**
     * To be called in response to [Lifecycle.Event.ON_START]. See [EngineView]
     * implementations for details.
     */
    fun onStart() = Unit

    /**
     * To be called in response to [Lifecycle.Event.ON_STOP]. See [EngineView]
     * implementations for details.
     */
    fun onStop() = Unit

    /**
     * To be called in response to [Lifecycle.Event.ON_CREATE]. See [EngineView]
     * implementations for details.
     */
    fun onCreate() = Unit

    /**
     * To be called in response to [Lifecycle.Event.ON_DESTROY]. See [EngineView]
     * implementations for details.
     */
    fun onDestroy() = Unit

    /**
     * Check if [EngineView] can be scrolled vertically up.
     * true if can and false otherwise.
     */
    fun canScrollVerticallyUp(): Boolean = true

    /**
     * Check if [EngineView] can be scrolled vertically down.
     * true if can and false otherwise.
     */
    fun canScrollVerticallyDown(): Boolean = true

    /**
     * Request a screenshot of the visible portion of the web page currently being rendered.
     * @param onFinish A callback to inform that process of capturing a thumbnail has finished.
     */
    fun captureThumbnail(onFinish: (Bitmap?) -> Unit)

    /**
     * Updates the amount of vertical space that is clipped or visibly obscured in the bottom portion of the view.
     * Tells the [EngineView] where to put bottom fixed elements so they are fully visible.
     *
     * @param clippingHeight The height of the bottom clipped space in screen pixels.
     */
    fun setVerticalClipping(clippingHeight: Int)
}

/**
 * [LifecycleObserver] which dispatches lifecycle events to an [EngineView].
 */
class LifecycleObserver(val engineView: EngineView) : androidx.lifecycle.LifecycleObserver {

    @OnLifecycleEvent(Lifecycle.Event.ON_PAUSE)
    fun onPause() {
        engineView.onPause()
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_RESUME)
    fun onResume() {
        engineView.onResume()
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_START)
    fun onStart() {
        engineView.onStart()
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
    fun onStop() {
        engineView.onStop()
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_CREATE)
    fun onCreate() {
        engineView.onCreate()
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_DESTROY)
    fun onDestroy() {
        engineView.onDestroy()
    }
}
