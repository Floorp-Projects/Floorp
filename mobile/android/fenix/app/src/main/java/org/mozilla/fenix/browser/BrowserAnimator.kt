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
    private val firstContentfulHappened: () -> Boolean
) {

    private val unwrappedEngineView: EngineView?
        get() = engineView.get()

    private val unwrappedSwipeRefresh: View?
        get() = swipeRefresh.get()

    fun beginAnimateInIfNecessary() {
        if (unwrappedSwipeRefresh?.context?.settings()?.waitToShowPageUntilFirstPaint == true) {
            if (firstContentfulHappened()) {
                viewLifecycleScope.get()?.launch {
                    delay(ANIMATION_DELAY)
                    unwrappedEngineView?.asView()?.visibility = View.VISIBLE
                    unwrappedSwipeRefresh?.background = null
                    unwrappedSwipeRefresh?.alpha = 1f
                }
            }
        } else {
            unwrappedSwipeRefresh?.alpha = 1f
            unwrappedEngineView?.asView()?.visibility = View.VISIBLE
            unwrappedSwipeRefresh?.background = null
        }
    }

    /**
     * Makes the swipeRefresh background a screenshot of the engineView in its current state.
     * This allows us to "animate" the engineView.
     */
    fun captureEngineViewAndDrawStatically(onComplete: () -> Unit) {
        unwrappedEngineView?.asView()?.context.let {
            viewLifecycleScope.get()?.launch {
                // isAdded check is necessary because of a bug in viewLifecycleOwner. See AC#3828
                if (!fragment.isAdded()) {
                    return@launch
                }
                unwrappedEngineView?.captureThumbnail { bitmap ->
                    if (!fragment.isAdded()) {
                        return@captureThumbnail
                    }

                    unwrappedSwipeRefresh?.apply {
                        // If the bitmap is null, the best we can do to reduce the flash is set transparent
                        background = bitmap?.toDrawable(context.resources)
                            ?: ColorDrawable(Color.TRANSPARENT)
                    }

                    unwrappedEngineView?.asView()?.visibility = View.GONE

                    onComplete()
                }
            }
        }
    }

    private fun WeakReference<Fragment>.isAdded(): Boolean {
        val unwrapped = get()
        return unwrapped != null && unwrapped.isAdded
    }

    companion object {
        private const val ANIMATION_DELAY = 100L

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
