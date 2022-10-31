/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.content.Context
import android.content.res.ColorStateList
import android.view.View
import androidx.annotation.ColorRes
import androidx.annotation.DrawableRes
import androidx.appcompat.widget.AppCompatImageView
import androidx.appcompat.widget.SwitchCompat
import androidx.core.view.isVisible
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuHighlight
import mozilla.components.browser.menu.HighlightableMenuItem
import mozilla.components.browser.menu.R
import mozilla.components.concept.menu.candidate.CompoundMenuCandidate
import mozilla.components.concept.menu.candidate.DrawableMenuIcon
import mozilla.components.concept.menu.candidate.LowPriorityHighlightEffect

/**
 * A browser menu switch that can show a highlighted icon.
 *
 * @param label The visible label of this menu item.
 * @param isCollapsingMenuLimit Whether this menu item can serve as the limit of a collapsing menu.
 * @param isSticky whether this item menu should not be scrolled offscreen (downwards or upwards
 * depending on the menu position).
 * @param initialState The initial value the checkbox should have.
 * @param listener Callback to be invoked when this menu item is checked.
 */
@Suppress("LongParameterList")
class BrowserMenuHighlightableSwitch(
    label: String,
    @DrawableRes private val startImageResource: Int,
    @ColorRes private val iconTintColorResource: Int = NO_ID,
    @ColorRes private val textColorResource: Int = NO_ID,
    override val isCollapsingMenuLimit: Boolean = false,
    override val isSticky: Boolean = false,
    override val highlight: BrowserMenuHighlight.LowPriority,
    override val isHighlighted: () -> Boolean = { true },
    initialState: () -> Boolean = { false },
    listener: (Boolean) -> Unit,
) : BrowserMenuCompoundButton(label, isCollapsingMenuLimit, isSticky, initialState, listener), HighlightableMenuItem {

    private var wasHighlighted = false

    override fun getLayoutResource(): Int = R.layout.mozac_browser_menu_highlightable_switch

    override fun bind(menu: BrowserMenu, view: View) {
        super.bind(menu, view.findViewById<SwitchCompat>(R.id.switch_widget))
        setTints(view)

        val notificationDotView = view.findViewById<AppCompatImageView>(R.id.notification_dot)
        notificationDotView.imageTintList = ColorStateList.valueOf(highlight.notificationTint)

        wasHighlighted = isHighlighted()
        updateHighlight(view, wasHighlighted)
    }

    override fun invalidate(view: View) {
        val isNowHighlighted = isHighlighted()
        if (isNowHighlighted != wasHighlighted) {
            wasHighlighted = isNowHighlighted
            updateHighlight(view, isNowHighlighted)
        }
    }

    private fun setTints(view: View) {
        val switch = view.findViewById<SwitchCompat>(R.id.switch_widget)
        switch.setColorResource(textColorResource)

        val imageView = view.findViewById<AppCompatImageView>(R.id.image)
        imageView.setImageResource(startImageResource)
        imageView.setTintResource(iconTintColorResource)
    }

    private fun updateHighlight(view: View, isHighlighted: Boolean) {
        val notificationDotView = view.findViewById<AppCompatImageView>(R.id.notification_dot)
        val switch = view.findViewById<SwitchCompat>(R.id.switch_widget)

        notificationDotView.isVisible = isHighlighted
        switch.text = if (isHighlighted) highlight.label ?: label else label
    }

    override fun asCandidate(context: Context): CompoundMenuCandidate {
        val base = super.asCandidate(context)
        return if (isHighlighted()) {
            base.copy(
                text = highlight.label ?: label,
                start = (base.start as? DrawableMenuIcon)?.copy(
                    effect = LowPriorityHighlightEffect(notificationTint = highlight.notificationTint),
                ),
            )
        } else {
            base
        }
    }
}
