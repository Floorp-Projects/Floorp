/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.animation.ValueAnimator
import android.content.Context
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.os.Bundle
import android.view.View
import android.view.animation.DecelerateInterpolator
import androidx.core.animation.doOnEnd
import androidx.core.graphics.drawable.toDrawable
import androidx.fragment.app.Fragment
import androidx.lifecycle.LifecycleCoroutineScope
import androidx.navigation.NavOptions
import kotlinx.coroutines.Dispatchers
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
    private val arguments: Bundle,
    private val firstContentfulHappened: () -> Boolean
) {

    private val unwrappedEngineView: EngineView?
        get() = engineView.get()

    private val unwrappedSwipeRefresh: View?
        get() = swipeRefresh.get()

    private val browserZoomInValueAnimator = ValueAnimator.ofFloat(0f, END_ANIMATOR_VALUE).apply {
        addUpdateListener {
            unwrappedSwipeRefresh?.scaleX =
                STARTING_XY_SCALE + XY_SCALE_MULTIPLIER * it.animatedFraction
            unwrappedSwipeRefresh?.scaleY =
                STARTING_XY_SCALE + XY_SCALE_MULTIPLIER * it.animatedFraction
            unwrappedSwipeRefresh?.alpha = it.animatedFraction
        }

        doOnEnd {
            if (firstContentfulHappened()) {
                unwrappedEngineView?.asView()?.visibility = View.VISIBLE
            }
            unwrappedSwipeRefresh?.background = null
            arguments.putBoolean(SHOULD_ANIMATE_FLAG, false)
        }

        interpolator = DecelerateInterpolator()
        duration = ANIMATION_DURATION
    }

    /**
     * Triggers the *zoom in* browser animation to run if necessary (based on the SHOULD_ANIMATE_FLAG).
     * Also removes the flag from the bundle so it is not played on future entries into the fragment.
     */
    fun beginAnimateInIfNecessary() {
        val shouldAnimate = arguments.getBoolean(SHOULD_ANIMATE_FLAG, false)
        if (shouldAnimate) {
            viewLifecycleScope.get()?.launch(Dispatchers.Main) {
                delay(ANIMATION_DELAY)
                captureEngineViewAndDrawStatically {
                    unwrappedSwipeRefresh?.alpha = 0f
                    browserZoomInValueAnimator.start()
                }
            }
        } else {
            unwrappedSwipeRefresh?.alpha = 1f
            if (firstContentfulHappened()) {
                unwrappedEngineView?.asView()?.visibility = View.VISIBLE
            }
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
        private const val SHOULD_ANIMATE_FLAG = "shouldAnimate"
        private const val ANIMATION_DELAY = 50L
        private const val ANIMATION_DURATION = 115L
        private const val END_ANIMATOR_VALUE = 500f
        private const val XY_SCALE_MULTIPLIER = .05f
        private const val STARTING_XY_SCALE = .95f

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
