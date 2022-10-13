/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.awesomebar

import androidx.compose.material.ContentAlpha
import androidx.compose.material.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

/**
 * Contains the default values used by the AwesomeBar.
 */
object AwesomeBarDefaults {
    /**
     * Creates an [AwesomeBarColors] that represents the default colors used in an AwesomeBar.
     *
     * @param background The background of the AwesomeBar.
     * @param title The text color for the title of a suggestion.
     * @param description The text color for the description of a suggestion.
     */
    @Composable
    fun colors(
        background: Color = MaterialTheme.colors.background,
        title: Color = MaterialTheme.colors.onBackground,
        description: Color = MaterialTheme.colors.onBackground.copy(
            alpha = ContentAlpha.medium,
        ),
        autocompleteIcon: Color = MaterialTheme.colors.onSurface,
        groupTitle: Color = MaterialTheme.colors.onBackground,
    ) = AwesomeBarColors(
        background,
        title,
        description,
        autocompleteIcon,
        groupTitle,
    )
}
