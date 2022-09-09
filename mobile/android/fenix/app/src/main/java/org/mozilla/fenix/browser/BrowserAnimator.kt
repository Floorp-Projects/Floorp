/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.content.Context
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.view.View
import androidx.core.graphics.drawable.toDrawable
import androidx.fragment.app.Fragment
import androidx.lifecycle.LifecycleCoroutineScope
import androidx.navigation.NavOptions
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import mozilla.components.concept.engine.EngineView
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.ext.settings
import java.lang.ref.WeakReference

/**
 * Handles properly animating the browser engine based on `SHOULD_ANIMATE_FLAG` passed in through
 * nav arguments.
 */
class BrowserAnimator(
    private val fragment: WeakReference<Fragment>,
    private val engineView: WeakReference<EngineView>,
    private val swipeRefresh: WeakReference<View>,
    private val viewLifecycleScope: WeakReference<LifecycleCoroutineScope>,
) {

    private val unwrappedEngineView: EngineView?
        get() = engineView.get()

    private val unwrappedSwipeRefresh: View?
        get() = swipeRefresh.get()

    fun beginAnimateInIfNecessary() {
        unwrappedEngineView?.asView()?.visibility = View.VISIBLE
        unwrappedSwipeRefresh?.background = null
    }

    /**
     * Captures a screenshot of the current web page and sets the bitmap
     * as a background of the engine view's parent (the swipeRefresh view).
     *
     * This is a workaround to prevent the home screen from flashing behind
     * the browser fragment when navigating away. We've also found this to
     * make transitions from the browser fragment to the home fragment
     * smoother. So, in addition, we are currently also using it as a
     * workaround to prevent flashes during those navigations.
     *
     * @param timeoutInMs timeout in milliseconds after which the operation
     * should be cancelled.
     * @param onComplete callback invoked when operation is completed or
     * cancelled, see [timeoutInMs]. The boolean passed to the lambda
     * indicates whether or not the operation was successful i.e., true,
     * if the bitmap was successfully set as a background, otherwise false.
     */
    fun captureEngineViewAndDrawStatically(timeoutInMs: Long = 250L, onComplete: (Boolean) -> Unit) {
        unwrappedEngineView?.asView()?.context.let {
            viewLifecycleScope.get()?.launch {
                var cancelled = false
                var completed = false
                // isAdded check is necessary because of a bug in viewLifecycleOwner. See AC#3828
                if (!fragment.isAdded()) {
                    return@launch
                }
                unwrappedEngineView?.captureThumbnail { bitmap ->
                    if (!fragment.isAdded() || cancelled) {
                        return@captureThumbnail
                    }

                    unwrappedSwipeRefresh?.apply {
                        // If the bitmap is null, the best we can do to reduce the flash is set transparent
                        background = bitmap?.toDrawable(context.resources)
                            ?: ColorDrawable(Color.TRANSPARENT)
                    }

                    unwrappedEngineView?.asView()?.visibility = View.GONE
                    completed = true
                    onComplete(bitmap != null)
                }
                delay(timeoutInMs)
                if (!completed) {
                    cancelled = true
                    onComplete(false)
                }
            }
        }
    }

    private fun WeakReference<Fragment>.isAdded(): Boolean {
        val unwrapped = get()
        return unwrapped != null && unwrapped.isAdded
    }

    companion object {
        fun getToolbarNavOptions(context: Context): NavOptions {
            val navOptions = NavOptions.Builder()

            when (context.settings().toolbarPosition) {
                ToolbarPosition.TOP -> {
                    navOptions.setEnterAnim(R.anim.fade_in)
                    navOptions.setExitAnim(R.anim.fade_out)
                }
                ToolbarPosition.BOTTOM -> {
                    navOptions.setEnterAnim(R.anim.fade_in_up)
                    navOptions.setExitAnim(R.anim.fade_out_down)
                }
            }

            return navOptions.build()
        }
    }
}
