/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar

import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.TextField
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType

/**
 * Sub-component of the [BrowserToolbar] responsible for allowing the user to edit the current
 * URL ("edit mode").
 *
 * @param url The initial URL to be edited.
 * @param onUrlCommitted Will be called when the user has finished editing and wants to initiate
 * loading the entered URL.
 */
@Composable
fun BrowserEditToolbar(
    url: String,
    onUrlEdit: (String) -> Unit = {},
    onUrlCommitted: (String) -> Unit = {}
) {
    TextField(
        url,
        onValueChange = { value ->
            onUrlEdit(value)
        },
        singleLine = true,
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Uri, imeAction = ImeAction.Go),
        keyboardActions = KeyboardActions(
            onGo = { onUrlCommitted(url) }
        ),
        modifier = Modifier.fillMaxWidth()
    )
}
