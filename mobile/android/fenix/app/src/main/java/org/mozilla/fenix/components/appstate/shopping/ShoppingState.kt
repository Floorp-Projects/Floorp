/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate.shopping

/**
 * State for shopping feature that's required to live the lifetime of a session.
 *
 * @property productsInAnalysis Set of product ids that are currently being analysed or were being
 * analysed when the sheet was closed.
 * @property shoppingSheetExpanded Boolean indicating if the shopping sheet is expanded and visible.
 */
data class ShoppingState(
    val productsInAnalysis: Set<String> = emptySet(),
    val shoppingSheetExpanded: Boolean? = null,
)
