/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.store

/**
 * States the review quality check bottom sheet can be opened in.
 *
 * @property state Name of the state to be used in review quality check telemetry.
 */
enum class BottomSheetViewState(val state: String) {
    FULL_VIEW("full"),
    HALF_VIEW("half"),
}

/**
 * The source of the bottom sheet dismiss.
 *
 * @property sourceName Name of the dismiss source to be used in review quality check telemetry.
 */
enum class BottomSheetDismissSource(val sourceName: String) {
    BACK_PRESSED("back_pressed"),
    SLIDE("sheet_slide"),
    HANDLE_CLICKED("handle_clicked"),
    CLICK_OUTSIDE("click_outside"),
    LINK_OPENED("link_opened"),
    OPT_OUT("opt_out"),
    NOT_NOW("not_now"),
}
