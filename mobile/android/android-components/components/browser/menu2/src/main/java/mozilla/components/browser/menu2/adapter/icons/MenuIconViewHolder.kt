/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.adapter.icons

import android.view.LayoutInflater
import android.view.View
import androidx.annotation.CallSuper
import androidx.annotation.LayoutRes
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.constraintlayout.widget.ConstraintSet
import androidx.constraintlayout.widget.ConstraintSet.END
import androidx.constraintlayout.widget.ConstraintSet.PARENT_ID
import androidx.constraintlayout.widget.ConstraintSet.START
import mozilla.components.browser.menu2.R
import mozilla.components.concept.menu.candidate.MenuIcon

/**
 * View holder with a [bind] method that passes the previously bound value.
 */
internal abstract class MenuIconViewHolder<T : MenuIcon>(
    protected val parent: ConstraintLayout,
    protected val inflater: LayoutInflater,
) {

    @Suppress("Unchecked_Cast")
    fun bindAndCast(newIcon: MenuIcon, oldIcon: MenuIcon?) {
        bind(newIcon as T, oldIcon as T?)
    }

    /**
     * Updates the held view to reflect changes in the menu option icon.
     *
     * @param newIcon New values to use.
     * @param oldIcon Previously set values.
     */
    protected abstract fun bind(newIcon: T, oldIcon: T?)

    /**
     * Inflates the layout resource and adds it to the parent layout.
     */
    protected fun inflate(@LayoutRes layoutResource: Int): View {
        val view = inflater.inflate(layoutResource, parent, false)
        parent.addView(view)
        return view
    }

    /**
     * Changes the constraints applied to [parent].
     */
    protected inline fun updateConstraints(update: ConstraintSet.() -> Unit) {
        ConstraintSet().apply {
            clone(parent)
            update()
            applyTo(parent)
        }
    }

    /**
     * Resets the layout and removes any child views.
     * Called when the view holder is removed.
     */
    @CallSuper
    open fun disconnect() {
        updateConstraints {
            connect(R.id.label, START, PARENT_ID, START)
            connect(R.id.label, END, PARENT_ID, END)
        }
    }
}
