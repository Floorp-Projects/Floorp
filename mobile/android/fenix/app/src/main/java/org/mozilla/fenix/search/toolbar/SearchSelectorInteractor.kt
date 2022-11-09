/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.toolbar

/**
 * Interface for search selector menu. This interface is implemented by objects that want
 * to respond to user interaction with items inside [SearchSelectorMenu].
 */
interface SearchSelectorInteractor {

    /**
     * Called when an user taps on a search selector menu item.
     *
     * @param item The [SearchSelectorMenu.Item] that was tapped.
     */
    fun onMenuItemTapped(item: SearchSelectorMenu.Item)
}
