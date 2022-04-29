/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.button

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Button
import androidx.compose.material.ButtonDefaults
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.mozilla.fenix.R
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

/**
 * Primary button.
 *
 * @param text The button text to be displayed.
 * @param icon Optional button icon to be displayed.
 * @param onClick Invoked when the user clicks on the button.
 */
@Composable
fun PrimaryButton(
    text: String,
    icon: Painter? = null,
    onClick: () -> Unit,
) {
    Button(
        onClick = onClick,
        modifier = Modifier
            .clip(RoundedCornerShape(size = 4.dp))
            .fillMaxWidth()
            .height(36.dp),
        elevation = ButtonDefaults.elevation(defaultElevation = 0.dp, pressedElevation = 0.dp),
        colors = ButtonDefaults.outlinedButtonColors(backgroundColor = FirefoxTheme.colors.actionPrimary),
    ) {
        if (icon != null) {
            Icon(
                painter = icon,
                contentDescription = null,
                tint = FirefoxTheme.colors.iconOnColor,
            )

            Spacer(modifier = Modifier.width(8.dp))
        }

        Text(
            text = text,
            color = FirefoxTheme.colors.textOnColorPrimary,
            fontSize = 14.sp,
            fontFamily = FontFamily(Font(R.font.metropolis_semibold)),
            letterSpacing = 0.sp,
            maxLines = 2
        )
    }
}

@Composable
@Preview
private fun PrimaryButtonPreview() {
    FirefoxTheme(theme = Theme.getTheme(isPrivate = false)) {
        Box(Modifier.background(FirefoxTheme.colors.layer1)) {
            PrimaryButton(
                text = stringResource(R.string.recent_tabs_show_all),
                icon = painterResource(R.drawable.ic_tab_collection),
                onClick = {},
            )
        }
    }
}
