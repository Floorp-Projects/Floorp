/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2

import android.view.View
import android.view.ViewGroup.LayoutParams.WRAP_CONTENT
import android.widget.PopupWindow
import mozilla.components.browser.menu2.ext.displayPopup
import mozilla.components.browser.menu2.view.MenuView
import mozilla.components.concept.menu.MenuController
import mozilla.components.concept.menu.MenuStyle
import mozilla.components.concept.menu.Orientation
import mozilla.components.concept.menu.Side
import mozilla.components.concept.menu.candidate.MenuCandidate
import mozilla.components.concept.menu.candidate.NestedMenuCandidate
import mozilla.components.concept.menu.ext.findNestedMenuCandidate
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry

/**
 * Controls a popup menu composed of MenuCandidate objects.
 * @param visibleSide Sets the menu to open with either the start or end visible.
 * @param style Custom styling for this menu controller.
 */
class BrowserMenuController(
    private val visibleSide: Side = Side.START,
    private val style: MenuStyle? = null,
) : MenuController, Observable<MenuController.Observer> by ObserverRegistry() {

    private var currentPopupInfo: PopupMenuInfo? = null
    private var menuCandidates: List<MenuCandidate> = emptyList()

    private val menuDismissListener = PopupWindow.OnDismissListener {
        currentPopupInfo = null
        notifyObservers { onDismiss() }
    }

    override fun show(
        anchor: View,
        orientation: Orientation?,
        autoDismiss: Boolean,
    ): PopupWindow {
        val view = MenuView(anchor.context).apply {
            // Show nested list if present, or the standard menu candidates list.
            submitList(menuCandidates)
            setVisibleSide(visibleSide)
            style?.let { setStyle(it) }
        }

        if (autoDismiss) {
            // Monitor for changes to the parent layout and dismiss pop up if displayed, else the menu
            // could be displayed in the wrong position. For example, if the menu is displayed and the
            // device orientation changes from portrait to landscape and vice versa.
            anchor.rootView.addOnLayoutChangeListener { _, _, _, right, bottom, _, _, oldRight, oldBottom ->
                if (bottom != oldBottom || right != oldRight) {
                    dismiss()
                }
            }
        }

        return MenuPopupWindow(view).apply {
            view.onDismiss = ::dismiss
            view.onReopenMenu = ::reopenMenu
            setOnDismissListener(menuDismissListener)
            displayPopup(view, anchor, orientation)
        }.also {
            currentPopupInfo = PopupMenuInfo(
                window = it,
                anchor = anchor,
                orientation = orientation,
                nested = null,
            )
        }
    }

    /**
     * Re-opens the menu and displays the given nested list.
     * No-op if the menu is not yet open.
     */
    private fun reopenMenu(nested: NestedMenuCandidate?) {
        val info = currentPopupInfo ?: return
        info.window.run {
            // Dismiss silently
            setOnDismissListener(null)
            dismiss()
            setOnDismissListener(menuDismissListener)

            // Quickly remove the current list
            view.submitList(null)
            // Display the new nested list
            view.submitList(nested?.subMenuItems ?: menuCandidates)
            // Reopen the menu
            displayPopup(view, info.anchor, info.orientation)
        }
        currentPopupInfo = info.copy(nested = nested)
    }

    /**
     * Dismiss the menu popup if the menu is visible.
     */
    override fun dismiss() {
        currentPopupInfo?.window?.dismiss()
    }

    /**
     * Changes the contents of the menu.
     */
    override fun submitList(list: List<MenuCandidate>) {
        menuCandidates = list
        val info = currentPopupInfo

        // If menu is already open, update the displayed items
        if (info != null) {
            // If a nested menu is open, it should be displayed
            val displayedItems = if (info.nested != null) {
                list.findNestedMenuCandidate(info.nested.id)?.subMenuItems
            } else {
                list
            }

            // If the new menu is null, close & reopen the popup on the main list
            if (displayedItems == null) {
                // close & reopen popup
                reopenMenu(nested = null)
            } else {
                info.window.view.submitList(displayedItems)
            }
        }

        notifyObservers { onMenuListSubmit(list) }
    }

    private class MenuPopupWindow(
        val view: MenuView,
    ) : PopupWindow(view, WRAP_CONTENT, WRAP_CONTENT, true)

    private data class PopupMenuInfo(
        val window: MenuPopupWindow,
        val anchor: View,
        val orientation: Orientation?,
        val nested: NestedMenuCandidate? = null,
    )
}
