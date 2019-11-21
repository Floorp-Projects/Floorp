/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu

import androidx.annotation.ColorInt
import androidx.annotation.ColorRes
import androidx.annotation.DrawableRes
import mozilla.components.browser.menu.item.NO_ID

/**
 * Describes how to display a [mozilla.components.browser.menu.item.BrowserMenuHighlightableItem]
 * when it is highlighted.
 */
sealed class BrowserMenuHighlight {
    abstract val label: String?

    /**
     * Displays a notification dot.
     * Used for highlighting new features to the user, such as what's new or a recommended feature.
     *
     * @property notificationTint Tint for the notification dot displayed on the icon and menu button.
     * @property label Label to override the normal label of the menu item.
     */
    data class LowPriority(
        @ColorInt val notificationTint: Int,
        override val label: String? = null
    ) : BrowserMenuHighlight()

    /**
     * Changes the background of the menu item.
     * Used for errors that require user attention, like sync errors.
     *
     * @property backgroundTint Tint for the menu item background color.
     * Also used to highlight the menu button.
     * @property label Label to override the normal label of the menu item.
     * @property endImageResource Icon to display at the end of the menu item when highlighted.
     */
    data class HighPriority(
        @ColorInt val backgroundTint: Int,
        override val label: String? = null,
        val endImageResource: Int = NO_ID
    ) : BrowserMenuHighlight()

    /**
     * Described how to display a highlightable menu item when it is highlighted.
     * Replaced by [LowPriority] and [HighPriority] which lets a priority be specified.
     * This class only exists so that [mozilla.components.browser.menu.item.BrowserMenuHighlightableItem.Highlight]
     * can subclass it.
     */
    @Deprecated("Replace with LowPriority or HighPriority highlight")
    open class ClassicHighlight(
        @DrawableRes val startImageResource: Int,
        @DrawableRes val endImageResource: Int,
        @DrawableRes val backgroundResource: Int,
        @ColorRes val colorResource: Int
    ) : BrowserMenuHighlight() {
        override val label: String? = null
    }
}
