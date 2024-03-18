/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.button

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
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
 * @param textColor [Color] to apply to the button text.
 */
@Composable
fun TextButton(
    text: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    textColor: Color = FirefoxTheme.colors.textAccent,
) {
    androidx.compose.material.TextButton(
        onClick = onClick,
        modifier = modifier,
    ) {
        Text(
            text = text.uppercase(Locale.getDefault()),
            color = textColor,
            style = FirefoxTheme.typography.button,
            maxLines = 1,
        )
    }
}

@Composable
@LightDarkPreview
private fun TextButtonPreview() {
    FirefoxTheme {
        Box(Modifier.background(FirefoxTheme.colors.layer1)) {
            TextButton(
                text = "label",
                onClick = {},
            )
        }
    }
}
