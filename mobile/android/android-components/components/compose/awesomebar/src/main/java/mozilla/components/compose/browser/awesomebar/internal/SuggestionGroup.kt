/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.awesomebar.internal

import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.compose.browser.awesomebar.AwesomeBarColors

/**
 * Renders a header for a group of suggestions.
 */
@Composable
internal fun SuggestionGroup(
    title: String,
    colors: AwesomeBarColors,
) {
    Text(
        title,
        color = colors.groupTitle,
        modifier = Modifier
            .padding(
                vertical = 12.dp,
                horizontal = 16.dp,
            )
            .fillMaxWidth(),
        fontSize = 14.sp,
    )
}
