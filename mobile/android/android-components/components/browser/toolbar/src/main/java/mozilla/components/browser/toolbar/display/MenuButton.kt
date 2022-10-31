/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.display

import androidx.annotation.ColorInt
import androidx.annotation.VisibleForTesting
import androidx.core.view.isVisible
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.ext.asCandidateList
import mozilla.components.browser.menu.ext.getHighlight
import mozilla.components.browser.toolbar.facts.emitOpenMenuFact
import mozilla.components.concept.menu.MenuController

internal class MenuButton(
    @get:VisibleForTesting internal val impl: mozilla.components.browser.menu.view.MenuButton,
) {

    init {
        impl.isVisible = false
        impl.register(
            object : mozilla.components.concept.menu.MenuButton.Observer {
                override fun onShow() {
                    emitOpenMenuFact(impl.menuBuilder?.extras)
                }
            },
        )
    }

    /**
     * Reference to the [MenuController].
     * If present, [menuBuilder] will be ignored.
     */
    var menuController: MenuController?
        get() = impl.menuController
        set(value) {
            impl.menuController = value
            impl.isVisible = shouldBeVisible()
        }

    /**
     * Legacy [BrowserMenuBuilder] reference.
     * Used to build the menu.
     */
    var menuBuilder: BrowserMenuBuilder?
        get() = impl.menuBuilder
        set(value) {
            impl.menuBuilder = value
            impl.isVisible = shouldBeVisible()
        }

    /**
     * Declare that the menu items should be updated if needed.
     * This should only be used once a [menuBuilder] is set.
     * To update items in the [menuController], call [MenuController.submitList] directly.
     */
    fun invalidateMenu() {
        val menuController = menuController
        if (menuController != null) {
            // Convert the menu builder items into a menu candidate list,
            // if the menu builder is present
            menuBuilder?.items?.let { items ->
                val list = items.asCandidateList(impl.context)
                menuController.submitList(list)
            }
        } else {
            // Invalidate the BrowserMenu
            impl.invalidateBrowserMenu()
            impl.setHighlight(menuBuilder?.items?.getHighlight())
        }
    }

    fun dismissMenu() {
        val menuController = menuController
        if (menuController != null) {
            // Use the controller to dismiss the menu
            menuController.dismiss()
        } else {
            // Use the button to dismiss the legacy menu
            impl.dismissMenu()
        }
    }

    /**
     * Sets a lambda to be invoked when the menu is dismissed
     */
    @Suppress("Deprecation")
    fun setMenuDismissAction(onDismiss: () -> Unit) {
        impl.onDismiss = onDismiss
    }

    fun setColorFilter(@ColorInt color: Int) = impl.setColorFilter(color)

    @VisibleForTesting
    internal fun shouldBeVisible() = impl.menuBuilder != null || impl.menuController != null
}
