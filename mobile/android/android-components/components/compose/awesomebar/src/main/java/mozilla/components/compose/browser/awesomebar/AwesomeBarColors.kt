/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.awesomebar

import androidx.compose.ui.graphics.Color

/**
 * Represents the colors used by the AwesomeBar.
 */
data class AwesomeBarColors(
    val background: Color,
    val title: Color,
    val description: Color,
    val autocompleteIcon: Color,
    val groupTitle: Color,
)
