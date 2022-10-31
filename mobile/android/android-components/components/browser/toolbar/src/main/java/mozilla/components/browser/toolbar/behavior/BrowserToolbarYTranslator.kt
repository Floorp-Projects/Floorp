/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.behavior

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.toolbar.BrowserToolbar

/**
 * Helper class with methods for translating on the Y axis a top / bottom [BottomToolbar].
 *
 * @param toolbarPosition whether the toolbar is displayed immediately at the top of the screen or
 * immediately at the bottom. This affects how it will be translated:
 *   - if place at the bottom it will be Y translated between 0 and [BrowserToolbar.getHeight]
 *   - if place at the top it will be Y translated between -[BrowserToolbar.getHeight] and 0
 */
class BrowserToolbarYTranslator(toolbarPosition: ToolbarPosition) {
    @VisibleForTesting
    internal var strategy = getTranslationStrategy(toolbarPosition)

    /**
     * Snap the [BrowserToolbar] to be collapsed or expanded, depending on whatever state is closer
     * over a short amount of time.
     */
    internal fun snapWithAnimation(toolbar: BrowserToolbar) {
        strategy.snapWithAnimation(toolbar)
    }

    /**
     * Snap the [BrowserToolbar] to be collapsed or expanded, depending on whatever state is closer immediately.
     */
    fun snapImmediately(toolbar: BrowserToolbar?) {
        strategy.snapImmediately(toolbar)
    }

    /**
     * Translate the [BrowserToolbar] to it's full visible height over a short amount of time.
     */
    internal fun expandWithAnimation(toolbar: BrowserToolbar) {
        strategy.expandWithAnimation(toolbar)
    }

    /**
     * Translate the [BrowserToolbar] to be hidden from view over a short amount of time.
     */
    internal fun collapseWithAnimation(toolbar: BrowserToolbar) {
        strategy.collapseWithAnimation(toolbar)
    }

    /**
     * Force expanding the [BrowserToolbar] depending on the [distance] value that should be translated
     * cancelling any other translation already in progress.
     */
    fun forceExpandIfNotAlready(toolbar: BrowserToolbar, distance: Float) {
        strategy.forceExpandWithAnimation(toolbar, distance)
    }

    /**
     * Translate [toolbar] immediately to the specified [distance] amount (positive or negative).
     */
    fun translate(toolbar: BrowserToolbar, distance: Float) {
        strategy.translate(toolbar, distance)
    }

    /**
     * Cancel any translation animations currently in progress.
     */
    fun cancelInProgressTranslation() {
        strategy.cancelInProgressTranslation()
    }

    @VisibleForTesting
    internal fun getTranslationStrategy(toolbarPosition: ToolbarPosition): BrowserToolbarYTranslationStrategy {
        return if (toolbarPosition == ToolbarPosition.TOP) {
            TopToolbarBehaviorStrategy()
        } else {
            BottomToolbarBehaviorStrategy()
        }
    }
}
