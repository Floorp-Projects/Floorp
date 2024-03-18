/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu

import android.content.Context
import androidx.annotation.ColorInt
import androidx.annotation.ColorRes
import androidx.annotation.DrawableRes
import androidx.core.content.ContextCompat
import mozilla.components.browser.menu.item.NO_ID
import mozilla.components.concept.menu.candidate.HighPriorityHighlightEffect
import mozilla.components.concept.menu.candidate.LowPriorityHighlightEffect
import mozilla.components.concept.menu.candidate.MenuEffect

/**
 * Describes how to display a [mozilla.components.browser.menu.item.BrowserMenuHighlightableItem]
 * when it is highlighted.
 */
sealed class BrowserMenuHighlight {
    abstract val label: String?
    abstract val canPropagate: Boolean

    /**
     * Converts the highlight into a corresponding [MenuEffect] from concept-menu.
     */
    abstract fun asEffect(context: Context): MenuEffect

    /**
     * Displays a notification dot.
     * Used for highlighting new features to the user, such as what's new or a recommended feature.
     *
     * @property notificationTint Tint for the notification dot displayed on the icon and menu button.
     * @property label Label to override the normal label of the menu item.
     * @property canPropagate Indicate whether other components should consider this highlight when
     * displaying their own highlight.
     */
    data class LowPriority(
        @ColorInt val notificationTint: Int,
        override val label: String? = null,
        override val canPropagate: Boolean = true,
    ) : BrowserMenuHighlight() {
        override fun asEffect(context: Context) = LowPriorityHighlightEffect(
            notificationTint = notificationTint,
        )
    }

    /**
     * Changes the background of the menu item.
     * Used for errors that require user attention, like sync errors.
     *
     * @property backgroundTint Tint for the menu item background color.
     * Also used to highlight the menu button.
     * @property label Label to override the normal label of the menu item.
     * @property endImageResource Icon to display at the end of the menu item when highlighted.
     * @property canPropagate Indicate whether other components should consider this highlight when
     * displaying their own highlight.
     */
    data class HighPriority(
        @ColorInt val backgroundTint: Int,
        override val label: String? = null,
        val endImageResource: Int = NO_ID,
        override val canPropagate: Boolean = true,
    ) : BrowserMenuHighlight() {
        override fun asEffect(context: Context) = HighPriorityHighlightEffect(
            backgroundTint = backgroundTint,
        )
    }

    /**
     * Described how to display a highlightable menu item when it is highlighted.
     * Replaced by [LowPriority] and [HighPriority] which lets a priority be specified.
     * This class only exists so that [mozilla.components.browser.menu.item.BrowserMenuHighlightableItem.Highlight]
     * can subclass it.
     *
     * @property canPropagate Indicate whether other components should consider this highlight when
     * displaying their own highlight.
     */
    @Deprecated("Replace with LowPriority or HighPriority highlight")
    open class ClassicHighlight(
        @DrawableRes val startImageResource: Int,
        @DrawableRes val endImageResource: Int,
        @DrawableRes val backgroundResource: Int,
        @ColorRes val colorResource: Int,
        override val canPropagate: Boolean = true,
    ) : BrowserMenuHighlight() {
        override val label: String? = null

        override fun asEffect(context: Context) = HighPriorityHighlightEffect(
            backgroundTint = ContextCompat.getColor(context, colorResource),
        )
    }
}

/**
 * Indicates that a menu item shows a highlight.
 */
interface HighlightableMenuItem {
    /**
     * Highlight object representing how the menu item will be displayed when highlighted.
     */
    val highlight: BrowserMenuHighlight

    /**
     * Whether or not to display the highlight
     */
    val isHighlighted: () -> Boolean
}
