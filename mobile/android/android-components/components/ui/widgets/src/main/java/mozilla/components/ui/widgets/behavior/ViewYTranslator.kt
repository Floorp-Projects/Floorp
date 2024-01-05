/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.widgets.behavior

import android.view.View
import androidx.annotation.VisibleForTesting

/**
 * Helper class with methods for translating on the Y axis a top / bottom [View].
 *
 * @param viewPosition whether the view is displayed immediately at the top of the screen or
 * immediately at the bottom. This affects how it will be translated:
 *   - if place at the bottom it will be Y translated between 0 and [View.getHeight]
 *   - if place at the top it will be Y translated between -[View.getHeight] and 0
 */
class ViewYTranslator(viewPosition: ViewPosition) {
    @VisibleForTesting
    internal var strategy = getTranslationStrategy(viewPosition)

    /**
     * Snap the [View] to be collapsed or expanded, depending on whatever state is closer
     * over a short amount of time.
     */
    internal fun snapWithAnimation(view: View) {
        strategy.snapWithAnimation(view)
    }

    /**
     * Snap the [View] to be collapsed or expanded, depending on whatever state is closer immediately.
     */
    fun snapImmediately(view: View?) {
        strategy.snapImmediately(view)
    }

    /**
     * Translate the [View] to it's full visible height over a short amount of time.
     */
    internal fun expandWithAnimation(view: View) {
        strategy.expandWithAnimation(view)
    }

    /**
     * Translate the [View] to be hidden from view over a short amount of time.
     */
    internal fun collapseWithAnimation(view: View) {
        strategy.collapseWithAnimation(view)
    }

    /**
     * Force expanding the [View] depending on the [distance] value that should be translated
     * cancelling any other translation already in progress.
     */
    fun forceExpandIfNotAlready(view: View, distance: Float) {
        strategy.forceExpandWithAnimation(view, distance)
    }

    /**
     * Translate [view] immediately to the specified [distance] amount (positive or negative).
     */
    fun translate(view: View, distance: Float) {
        strategy.translate(view, distance)
    }

    /**
     * Cancel any translation animations currently in progress.
     */
    fun cancelInProgressTranslation() {
        strategy.cancelInProgressTranslation()
    }

    @VisibleForTesting
    internal fun getTranslationStrategy(viewPosition: ViewPosition): ViewYTranslationStrategy {
        return if (viewPosition == ViewPosition.TOP) {
            TopViewBehaviorStrategy()
        } else {
            BottomViewBehaviorStrategy()
        }
    }
}
