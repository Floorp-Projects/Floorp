/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.identitycredential

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.feature.prompts.identitycredential.previews.DialogPreviewMaterialTheme
import mozilla.components.feature.prompts.identitycredential.previews.LightDarkPreview

/**
 * List item used to display an IdentityCredential item that supports clicks
 *
 * @param title the Title of the item
 * @param description The Description of the item.
 * @param modifier The modifier to apply to this layout.
 * @param onClick Invoked when the item is clicked.
 * @param beforeItemContent An optional layout to display before the item.
 *
 */
@Composable
internal fun IdentityCredentialItem(
    title: String,
    description: String,
    modifier: Modifier = Modifier,
    colors: DialogColors = DialogColors.default(),
    onClick: () -> Unit,
    beforeItemContent: (@Composable () -> Unit)? = null,
) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .clickable(onClick = onClick)
            .padding(horizontal = 16.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        beforeItemContent?.invoke()

        Column {
            Text(
                text = title,
                style = TextStyle(
                    fontSize = 16.sp,
                    lineHeight = 24.sp,
                    color = colors.title,
                    letterSpacing = 0.15.sp,
                ),
                maxLines = 1,
            )

            Text(
                text = description,
                style = TextStyle(
                    fontSize = 14.sp,
                    lineHeight = 20.sp,
                    color = colors.description,
                    letterSpacing = 0.25.sp,
                ),
                maxLines = 1,
            )
        }
    }
}

@Composable
@LightDarkPreview
private fun ProviderItemPreview() {
    DialogPreviewMaterialTheme {
        IdentityCredentialItem(
            modifier = Modifier.background(MaterialTheme.colors.background),
            title = "Title",
            description = "Description",
            onClick = {},
        )
    }
}

@Composable
@Preview(name = "Provider with a start-spacer")
private fun ProviderItemPreviewWithSpacer() {
    IdentityCredentialItem(
        modifier = Modifier.background(Color.White),
        title = "Title",
        description = "Description",
        onClick = {},
    ) {
        Spacer(modifier = Modifier.size(24.dp))
    }
}
