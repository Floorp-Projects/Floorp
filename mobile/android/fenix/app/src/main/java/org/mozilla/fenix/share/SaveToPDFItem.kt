/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.width
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A save to PDF item.
 *
 *  @param onClick event handler when the save to PDF item is clicked.
 */
@Composable
fun SaveToPDFItem(
    onClick: () -> Unit,
) {
    Row(
        modifier = Modifier
            .height(56.dp)
            .fillMaxWidth()
            .clickable(onClick = onClick),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Spacer(Modifier.width(16.dp))

        Icon(
            painter = painterResource(R.drawable.ic_download),
            contentDescription = stringResource(
                R.string.content_description_close_button,
            ),
            tint = FirefoxTheme.colors.iconPrimary,
        )

        Spacer(Modifier.width(32.dp))

        Text(
            color = FirefoxTheme.colors.textPrimary,
            text = stringResource(R.string.share_save_to_pdf),
            style = FirefoxTheme.typography.subtitle1,
        )
    }
}

@Composable
@Preview
@LightDarkPreview
private fun SaveToPDFItemPreview() {
    FirefoxTheme {
        SaveToPDFItem {}
    }
}
