/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.adapter.icons

import android.view.LayoutInflater
import androidx.annotation.VisibleForTesting
import androidx.constraintlayout.widget.ConstraintLayout
import mozilla.components.concept.menu.Side
import mozilla.components.concept.menu.candidate.AsyncDrawableMenuIcon
import mozilla.components.concept.menu.candidate.DrawableButtonMenuIcon
import mozilla.components.concept.menu.candidate.DrawableMenuIcon
import mozilla.components.concept.menu.candidate.MenuIcon
import mozilla.components.concept.menu.candidate.TextMenuIcon

/**
 * Helper class to manage a [ConstraintLayout] that can contain menu icon views.
 * Different holder classes are used to swap out the child views.
 */
internal class MenuIconAdapter(
    private val parent: ConstraintLayout,
    private val inflater: LayoutInflater,
    private val side: Side,
    private val dismiss: () -> Unit,
) {

    private var viewHolder: MenuIconViewHolder<out MenuIcon>? = null

    fun bind(newIcon: MenuIcon?, oldIcon: MenuIcon?) {
        if (newIcon == null && oldIcon != null) {
            viewHolder?.disconnect()
            viewHolder = null
        } else if (newIcon != null) {
            if (oldIcon == null || newIcon::class != oldIcon::class) {
                viewHolder?.disconnect()
                viewHolder = createViewHolder(newIcon)
            }

            viewHolder?.bindAndCast(newIcon, oldIcon)
        }
    }

    @VisibleForTesting
    internal fun createViewHolder(item: MenuIcon): MenuIconViewHolder<*> = when (item) {
        is DrawableMenuIcon -> DrawableMenuIconViewHolder(parent, inflater, side)
        is DrawableButtonMenuIcon -> DrawableButtonMenuIconViewHolder(parent, inflater, side, dismiss)
        is AsyncDrawableMenuIcon -> AsyncDrawableMenuIconViewHolder(parent, inflater, side)
        is TextMenuIcon -> TextMenuIconViewHolder(parent, inflater, side)
    }
}
