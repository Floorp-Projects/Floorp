/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.display

import androidx.annotation.ColorInt
import androidx.annotation.VisibleForTesting
import androidx.core.view.isVisible
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.ext.getHighlight
import mozilla.components.browser.toolbar.facts.emitOpenMenuFact

internal class MenuButton(
    @VisibleForTesting
    internal val impl: mozilla.components.browser.menu.view.MenuButton
) {

    init {
        impl.isVisible = false
        impl.onShow = {
            emitOpenMenuFact(impl.menuBuilder?.extras)
        }
    }

    var menuBuilder: BrowserMenuBuilder?
        get() = impl.menuBuilder
        set(value) {
            impl.menuBuilder = value
            impl.isVisible = value != null
        }

    /**
     * Declare that the menu items should be updated if needed.
     */
    @Suppress("Deprecation")
    fun invalidateMenu() {
        impl.invalidateBrowserMenu()
        impl.setHighlight(menuBuilder?.items?.getHighlight())
    }

    fun dismissMenu() = impl.dismissMenu()

    /**
     * Sets a lambda to be invoked when the menu is dismissed
     */
    fun setMenuDismissAction(onDismiss: () -> Unit) {
        impl.onDismiss = onDismiss
    }

    fun setColorFilter(@ColorInt color: Int) = impl.setColorFilter(color)
}
