/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu

import android.view.View

/**
 * Configuration of where and how a PopupWindow for a menu should be displayed.
 */
internal sealed class BrowserMenuPlacement {
    /**
     * Android View that the PopupWindow should be anchored to.
     */
    abstract val anchor: View

    /**
     * Menu position specific animation to be used when showing the PopupWindow.
     */
    abstract val animation: Int

    /**
     * Menu placed below the anchor. Anchored to the top.
     */
    class AnchoredToTop {
        /**
         * The PopupWindow should be anchored to the top and shown as a dropdown.
         */
        data class Dropdown(
            override val anchor: View,
            override val animation: Int = R.style.Mozac_Browser_Menu_Animation_OverflowMenuTop,
        ) : BrowserMenuPlacement()

        /**
         * The PopupWindow should be anchored to the top and placed at a specific location.
         */
        data class ManualAnchoring(
            override val anchor: View,
            override val animation: Int = R.style.Mozac_Browser_Menu_Animation_OverflowMenuTop,
        ) : BrowserMenuPlacement()
    }

    /**
     * Menu placed above the anchor. Anchored to the bottom.
     */
    class AnchoredToBottom {
        /**
         * The PopupWindow should be anchored to the bottom and shown as a dropdown.
         */
        data class Dropdown(
            override val anchor: View,
            override val animation: Int = R.style.Mozac_Browser_Menu_Animation_OverflowMenuBottom,
        ) : BrowserMenuPlacement()

        /**
         * The PopupWindow should be anchored to the bottom and placed at a specific location.
         */
        data class ManualAnchoring(
            override val anchor: View,
            override val animation: Int = R.style.Mozac_Browser_Menu_Animation_OverflowMenuBottom,
        ) : BrowserMenuPlacement()
    }
}
