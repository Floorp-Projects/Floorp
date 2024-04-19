/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.button

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme
import java.util.Locale

/**
 * Text-only button.
 *
 * @param text The button text to be displayed.
 * @param onClick Invoked when the user clicks on the button.
 * @param modifier [Modifier] Used to shape and position the underlying [androidx.compose.material.TextButton].
 * @param enabled Controls the enabled state of the button. When `false`, this button will not
 * be clickable.
 * @param textColor [Color] to apply to the button text.
 * @param upperCaseText If the button text should be in uppercase letters.
 */
@Composable
fun TextButton(
    text: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    textColor: Color = FirefoxTheme.colors.textAccent,
    upperCaseText: Boolean = true,
) {
    androidx.compose.material.TextButton(
        onClick = onClick,
        modifier = modifier,
        enabled = enabled,
    ) {
        Text(
            text = if (upperCaseText) {
                text.uppercase(Locale.getDefault())
            } else {
                text
            },
            color = if (enabled) textColor else FirefoxTheme.colors.textDisabled,
            style = FirefoxTheme.typography.button,
            maxLines = 1,
        )
    }
}

@Composable
@LightDarkPreview
private fun TextButtonPreview() {
    FirefoxTheme {
        Column(Modifier.background(FirefoxTheme.colors.layer1)) {
            TextButton(
                text = "label",
                onClick = {},
            )

            TextButton(
                text = "disabled",
                onClick = {},
                enabled = false,
            )
        }
    }
}
