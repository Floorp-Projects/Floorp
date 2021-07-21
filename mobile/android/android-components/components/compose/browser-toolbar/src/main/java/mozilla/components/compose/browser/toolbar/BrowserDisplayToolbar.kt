/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Button
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Text
import androidx.compose.material.contentColorFor
import androidx.compose.material.primarySurface
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

/**
 * Sub-component of the [BrowserToolbar] responsible for displaying the URL and related
 * controls ("display mode").
 *
 * @param url The URL to be displayed.
 * @param onUrlClicked Will be called when the user clicks on the URL.
 */
@Composable
fun BrowserDisplayToolbar(
    url: String,
    onUrlClicked: () -> Unit = {},
    onMenuClicked: () -> Unit = {}
) {
    val backgroundColor = MaterialTheme.colors.primarySurface
    val foregroundColor = contentColorFor(backgroundColor)

    Row(
        Modifier.background(backgroundColor)
    ) {
        Text(
            url,
            color = foregroundColor,
            modifier = Modifier
                .clickable { onUrlClicked() }
                .padding(8.dp)
                .fillMaxWidth(),
            maxLines = 1
        )
        Button(onClick = { onMenuClicked() }) {
            Text(":")
        }
    }
}
