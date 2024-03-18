/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.selection

/**
 * Interactor for items that can be selected on the bookmarks and history screens.
 */
interface SelectionInteractor<T> {
    /**
     * Called when an item is tapped to open it.
     * @param item the tapped item to open.
     */
    fun open(item: T)

    /**
     * Called when an item is long pressed and selection mode is started,
     * or when selection mode has already started an an item is tapped.
     * @param item the item to select.
     */
    fun select(item: T)

    /**
     * Called when a selected item is tapped in selection mode and should no longer be selected.
     * @param item the item to deselect.
     */
    fun deselect(item: T)
}
