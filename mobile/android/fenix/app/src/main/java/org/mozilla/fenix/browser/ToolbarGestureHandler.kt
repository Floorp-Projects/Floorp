/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.animation.Animator
import android.animation.AnimatorListenerAdapter
import android.app.Activity
import android.graphics.PointF
import android.graphics.Rect
import android.util.TypedValue
import android.view.View
import android.view.ViewConfiguration
import androidx.annotation.Dimension
import androidx.annotation.Dimension.DP
import androidx.core.graphics.contains
import androidx.core.graphics.toPoint
import androidx.core.view.isVisible
import androidx.dynamicanimation.animation.DynamicAnimation
import androidx.dynamicanimation.animation.FlingAnimation
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.support.ktx.android.util.dpToPx
import mozilla.components.support.ktx.android.view.getRectWithViewLocation
import org.mozilla.fenix.ext.getRectWithScreenLocation
import org.mozilla.fenix.ext.getWindowInsets
import org.mozilla.fenix.ext.isKeyboardVisible
import org.mozilla.fenix.ext.sessionsOfType
import org.mozilla.fenix.ext.settings
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min

/**
 * Handles intercepting touch events on the toolbar for swipe gestures and executes the
 * necessary animations.
 */
@Suppress("LargeClass", "TooManyFunctions")
class ToolbarGestureHandler(
    private val activity: Activity,
    private val contentLayout: View,
    private val tabPreview: TabPreview,
    private val toolbarLayout: View,
    private val sessionManager: SessionManager
) : SwipeGestureListener {

    private enum class GestureDirection {
        LEFT_TO_RIGHT, RIGHT_TO_LEFT
    }

    private sealed class Destination {
        data class Tab(val session: Session) : Destination()
        object None : Destination()
    }

    private val windowWidth: Int
        get() = activity.resources.displayMetrics.widthPixels

    private val previewOffset = PREVIEW_OFFSET.dpToPx(activity.resources.displayMetrics)

    private val touchSlop = ViewConfiguration.get(activity).scaledTouchSlop
    private val minimumFlingVelocity = ViewConfiguration.get(activity).scaledMinimumFlingVelocity
    private val defaultVelocity = TypedValue.applyDimension(
        TypedValue.COMPLEX_UNIT_DIP,
        MINIMUM_ANIMATION_VELOCITY,
        activity.resources.displayMetrics
    )

    private var gestureDirection = GestureDirection.LEFT_TO_RIGHT

    override fun onSwipeStarted(start: PointF, next: PointF): Boolean {
        val dx = next.x - start.x
        val dy = next.y - start.y
        gestureDirection = if (dx < 0) {
            GestureDirection.RIGHT_TO_LEFT
        } else {
            GestureDirection.LEFT_TO_RIGHT
        }

        @Suppress("ComplexCondition")
        return if (
            !activity.window.decorView.isKeyboardVisible() &&
            start.isInToolbar() &&
            abs(dx) > touchSlop &&
            abs(dy) < abs(dx)
        ) {
            preparePreview(getDestination())
            true
        } else {
            false
        }
    }

    override fun onSwipeUpdate(distanceX: Float, distanceY: Float) {
        when (getDestination()) {
            is Destination.Tab -> {
                // Restrict the range of motion for the views so you can't start a swipe in one direction
                // then move your finger far enough in the other direction and make the content visually
                // start sliding off screen the other way.
                tabPreview.translationX = when (gestureDirection) {
                    GestureDirection.RIGHT_TO_LEFT -> min(
                        windowWidth.toFloat() + previewOffset,
                        tabPreview.translationX - distanceX
                    )
                    GestureDirection.LEFT_TO_RIGHT -> max(
                        -windowWidth.toFloat() - previewOffset,
                        tabPreview.translationX - distanceX
                    )
                }
                contentLayout.translationX = when (gestureDirection) {
                    GestureDirection.RIGHT_TO_LEFT -> min(
                        0f,
                        contentLayout.translationX - distanceX
                    )
                    GestureDirection.LEFT_TO_RIGHT -> max(
                        0f,
                        contentLayout.translationX - distanceX
                    )
                }
            }
            is Destination.None -> {
                // If there is no "next" tab to swipe to in the gesture direction, only do a
                // partial animation to show that we are at the end of the tab list
                val maxContentHidden = contentLayout.width * OVERSCROLL_HIDE_PERCENT
                contentLayout.translationX = when (gestureDirection) {
                    GestureDirection.RIGHT_TO_LEFT -> max(
                        -maxContentHidden.toFloat(),
                        contentLayout.translationX - distanceX
                    ).coerceAtMost(0f)
                    GestureDirection.LEFT_TO_RIGHT -> min(
                        maxContentHidden.toFloat(),
                        contentLayout.translationX - distanceX
                    ).coerceAtLeast(0f)
                }
            }
        }
    }

    override fun onSwipeFinished(
        velocityX: Float,
        velocityY: Float
    ) {
        val destination = getDestination()
        if (destination is Destination.Tab && isGestureComplete(velocityX)) {
            animateToNextTab(velocityX, destination.session)
        } else {
            animateCanceledGesture(velocityX)
        }
    }

    private fun createFlingAnimation(
        view: View,
        minValue: Float,
        maxValue: Float,
        startVelocity: Float
    ): FlingAnimation =
        FlingAnimation(view, DynamicAnimation.TRANSLATION_X).apply {
            setMinValue(minValue)
            setMaxValue(maxValue)
            setStartVelocity(startVelocity)
            friction = ViewConfiguration.getScrollFriction()
        }

    private fun getDestination(): Destination {
        val isLtr = activity.resources.configuration.layoutDirection == View.LAYOUT_DIRECTION_LTR
        val currentSession = sessionManager.selectedSession ?: return Destination.None
        val currentIndex = sessionManager.sessionsOfType(currentSession.private).indexOfFirst {
            it.id == currentSession.id
        }

        return if (currentIndex == -1) {
            Destination.None
        } else {
            val sessions = sessionManager.sessionsOfType(currentSession.private)
            val index = when (gestureDirection) {
                GestureDirection.RIGHT_TO_LEFT -> if (isLtr) {
                    currentIndex - 1
                } else {
                    currentIndex + 1
                }
                GestureDirection.LEFT_TO_RIGHT -> if (isLtr) {
                    currentIndex + 1
                } else {
                    currentIndex - 1
                }
            }

            if (index < sessions.count() && index >= 0) {
                Destination.Tab(sessions.elementAt(index))
            } else {
                Destination.None
            }
        }
    }

    private fun preparePreview(destination: Destination) {
        val thumbnailId = when (destination) {
            is Destination.Tab -> destination.session.id
            is Destination.None -> return
        }

        tabPreview.loadPreviewThumbnail(thumbnailId)
        tabPreview.alpha = 1f
        tabPreview.translationX = when (gestureDirection) {
            GestureDirection.RIGHT_TO_LEFT -> windowWidth.toFloat() + previewOffset
            GestureDirection.LEFT_TO_RIGHT -> -windowWidth.toFloat() - previewOffset
        }
        tabPreview.isVisible = true
    }

    /**
     * Checks if the gesture is complete based on the position of tab preview and the velocity of
     * the gesture. A completed gesture means the user has indicated they want to swipe to the next
     * tab. The gesture is considered complete if one of the following is true:
     *
     * 1. The user initiated a fling in the same direction as the initial movement
     * 2. There is no fling initiated, but the percentage of the tab preview shown is at least
     * [GESTURE_FINISH_PERCENT]
     *
     * If the user initiated a fling in the opposite direction of the initial movement, the
     * gesture is always considered incomplete.
     */
    private fun isGestureComplete(velocityX: Float): Boolean {
        val previewWidth = tabPreview.getRectWithViewLocation().visibleWidth.toDouble()
        val velocityMatchesDirection = when (gestureDirection) {
            GestureDirection.RIGHT_TO_LEFT -> velocityX <= 0
            GestureDirection.LEFT_TO_RIGHT -> velocityX >= 0
        }
        val reverseFling =
            abs(velocityX) >= minimumFlingVelocity && !velocityMatchesDirection

        return !reverseFling && (previewWidth / windowWidth >= GESTURE_FINISH_PERCENT ||
            abs(velocityX) >= minimumFlingVelocity)
    }

    private fun getVelocityFromFling(velocityX: Float): Float {
        return max(abs(velocityX), defaultVelocity)
    }

    private fun animateToNextTab(velocityX: Float, session: Session) {
        val browserFinalXCoordinate: Float = when (gestureDirection) {
            GestureDirection.RIGHT_TO_LEFT -> -windowWidth.toFloat() - previewOffset
            GestureDirection.LEFT_TO_RIGHT -> windowWidth.toFloat() + previewOffset
        }
        val animationVelocity = when (gestureDirection) {
            GestureDirection.RIGHT_TO_LEFT -> -getVelocityFromFling(velocityX)
            GestureDirection.LEFT_TO_RIGHT -> getVelocityFromFling(velocityX)
        }

        // Finish animating the contentLayout off screen and tabPreview on screen
        createFlingAnimation(
            view = contentLayout,
            minValue = min(0f, browserFinalXCoordinate),
            maxValue = max(0f, browserFinalXCoordinate),
            startVelocity = animationVelocity
        ).addUpdateListener { _, value, _ ->
            tabPreview.translationX = when (gestureDirection) {
                GestureDirection.RIGHT_TO_LEFT -> value + windowWidth + previewOffset
                GestureDirection.LEFT_TO_RIGHT -> value - windowWidth - previewOffset
            }
        }.addEndListener { _, _, _, _ ->
            contentLayout.translationX = 0f
            sessionManager.select(session)

            // Fade out the tab preview to prevent flickering
            val shortAnimationDuration =
                activity.resources.getInteger(android.R.integer.config_shortAnimTime)
            tabPreview.animate()
                .alpha(0f)
                .setDuration(shortAnimationDuration.toLong())
                .setListener(object : AnimatorListenerAdapter() {
                    override fun onAnimationEnd(animation: Animator?) {
                        tabPreview.isVisible = false
                    }
                })
        }.start()
    }

    private fun animateCanceledGesture(gestureVelocity: Float) {
        val velocity = if (getDestination() is Destination.None) {
            defaultVelocity
        } else {
            getVelocityFromFling(gestureVelocity)
        }.let { v ->
            when (gestureDirection) {
                GestureDirection.RIGHT_TO_LEFT -> v
                GestureDirection.LEFT_TO_RIGHT -> -v
            }
        }

        createFlingAnimation(
            view = contentLayout,
            minValue = min(0f, contentLayout.translationX),
            maxValue = max(0f, contentLayout.translationX),
            startVelocity = velocity
        ).addUpdateListener { _, value, _ ->
            tabPreview.translationX = when (gestureDirection) {
                GestureDirection.RIGHT_TO_LEFT -> value + windowWidth + previewOffset
                GestureDirection.LEFT_TO_RIGHT -> value - windowWidth - previewOffset
            }
        }.addEndListener { _, _, _, _ ->
            tabPreview.isVisible = false
        }.start()
    }

    private fun PointF.isInToolbar(): Boolean {
        val toolbarLocation = toolbarLayout.getRectWithScreenLocation()
        // In Android 10, the system gesture touch area overlaps the bottom of the toolbar, so
        // lets make our swipe area taller by that amount
        activity.window.decorView.getWindowInsets()?.let { insets ->
            if (activity.settings().shouldUseBottomToolbar) {
                toolbarLocation.top -= (insets.mandatorySystemGestureInsets.bottom - insets.stableInsetBottom)
            }
        }
        return toolbarLocation.contains(toPoint())
    }

    private val Rect.visibleWidth: Int
        get() = if (left < 0) {
            right
        } else {
            windowWidth - left
        }

    companion object {
        /**
         * The percentage of the tab preview that needs to be visible to consider the
         * tab switching gesture complete.
         */
        private const val GESTURE_FINISH_PERCENT = 0.25

        /**
         * The percentage of the content view that can be hidden by the tab switching gesture if
         * there is not tab available to switch to
         */
        private const val OVERSCROLL_HIDE_PERCENT = 0.20

        /**
         * The speed of the fling animation (in dp per second).
         */
        @Dimension(unit = DP)
        private const val MINIMUM_ANIMATION_VELOCITY = 1500f

        /**
         * The size of the gap between the tab preview and content layout.
         */
        @Dimension(unit = DP)
        private const val PREVIEW_OFFSET = 48
    }
}
