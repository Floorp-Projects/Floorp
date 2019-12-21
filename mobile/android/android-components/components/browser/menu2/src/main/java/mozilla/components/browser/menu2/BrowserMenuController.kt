/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2

import mozilla.components.concept.menu.Side
import android.view.View
import android.view.ViewGroup.LayoutParams.WRAP_CONTENT
import android.widget.PopupWindow
import androidx.annotation.Px
import androidx.coordinatorlayout.widget.CoordinatorLayout
import mozilla.components.browser.menu2.ext.displayPopup
import mozilla.components.browser.menu2.view.MenuView
import mozilla.components.concept.menu.MenuController
import mozilla.components.concept.menu.Orientation
import mozilla.components.concept.menu.candidate.MenuCandidate
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry

/**
 * Controls a popup menu composed of MenuCandidate objects.
 * @param visibleSide Sets the menu to open with either the start or end visible.
 */
class BrowserMenuController(
    private val visibleSide: Side = Side.START
) : MenuController, Observable<MenuController.Observer> by ObserverRegistry() {

    private var menuCandidates: List<MenuCandidate> = emptyList()
    private var currentPopup: PopupWindow? = null

    /**
     * @param anchor The view on which to pin the popup window.
     * @param orientation The preferred orientation to show the popup window.
     * @param width The width of the popup menu. The height is always set to wrap content.
     */
    fun show(
        anchor: View,
        orientation: Orientation = determineMenuOrientation(anchor.parent as? View?),
        @Px width: Int = anchor.resources.getDimensionPixelSize(R.dimen.mozac_browser_menu2_width)
    ): PopupWindow {
        val view = MenuView(anchor.context).apply {
            submitList(menuCandidates)
            setVisibleSide(visibleSide)
        }

        return PopupWindow(view, width, WRAP_CONTENT, true).apply {
            view.onDismiss = ::dismiss
            setOnDismissListener {
                currentPopup = null
                notifyObservers { onDismiss() }
            }

            displayPopup(view, anchor, orientation)
        }.also {
            currentPopup = it
        }
    }

    override fun show(anchor: View): PopupWindow =
        show(anchor, orientation = determineMenuOrientation(anchor.parent as? View?))

    /**
     * Dismiss the menu popup if the menu is visible.
     */
    override fun dismiss() {
        currentPopup?.dismiss()
    }

    /**
     * Changes the contents of the menu.
     */
    override fun submitList(list: List<MenuCandidate>) {
        menuCandidates = list

        val openMenu = currentPopup?.contentView as MenuView?
        openMenu?.submitList(list)

        notifyObservers { onMenuListSubmit(list) }
    }
}

/**
 * Determines the orientation to be used for a menu
 * based on the positioning of the [parent] in the layout.
 */
fun determineMenuOrientation(parent: View?): Orientation {
    val params = parent?.layoutParams as? CoordinatorLayout.LayoutParams
        ?: return Orientation.DOWN

    return Orientation.fromGravity(params.gravity)
}
