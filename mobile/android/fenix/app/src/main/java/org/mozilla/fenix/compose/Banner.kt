/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.button.TextButton
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Default layout for a Banner messaging surface with two text buttons.
 *
 * @param message The primary text displayed to the user.
 * @param button1Text The text of the first button.
 * @param button2Text The text of the second button.
 * @param onButton1Click Invoked when the first button is clicked.
 * @param onButton2Click Invoked when the second button is clicked.
 */
@Composable
fun Banner(
    message: String,
    button1Text: String,
    button2Text: String,
    onButton1Click: () -> Unit,
    onButton2Click: () -> Unit,
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .background(FirefoxTheme.colors.layer1)
            .padding(all = 16.dp),
    ) {
        Text(
            text = message,
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.body2,
        )

        Spacer(modifier = Modifier.height(12.dp))

        Row(modifier = Modifier.align(Alignment.End)) {
            TextButton(
                text = button1Text,
                onClick = onButton2Click,
            )

            Spacer(modifier = Modifier.width(12.dp))

            TextButton(
                text = button2Text,
                onClick = onButton1Click,
            )
        }
    }
}

@LightDarkPreview
@Composable
private fun BannerPreview() {
    FirefoxTheme {
        Banner(
            message = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer sodales laoreet commodo.",
            button1Text = "Button 1",
            button2Text = "Button 2",
            onButton1Click = {},
            onButton2Click = {},
        )
    }
}
