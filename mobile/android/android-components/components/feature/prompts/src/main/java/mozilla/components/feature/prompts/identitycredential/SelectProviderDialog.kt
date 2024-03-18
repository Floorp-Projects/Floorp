/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.identitycredential

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.concept.identitycredential.Provider
import mozilla.components.feature.prompts.R
import mozilla.components.feature.prompts.identitycredential.previews.DialogPreviewMaterialTheme
import mozilla.components.feature.prompts.identitycredential.previews.LightDarkPreview
import mozilla.components.support.ktx.kotlin.base64ToBitmap

/**
 * A Federated Credential Management dialog for selecting a provider.
 * @param providers The list of available providers.
 * @param colors The colors of the dialog.
 * @param modifier [Modifier] to be applied to the layout.
 * @param onProviderClick Called when the user clicks on an item.
 */
@Composable
fun SelectProviderDialog(
    providers: List<Provider>,
    modifier: Modifier = Modifier,
    colors: DialogColors = DialogColors.default(),
    onProviderClick: (Provider) -> Unit,
) {
    Column(
        modifier = modifier.fillMaxWidth(),
    ) {
        Text(
            text = stringResource(id = R.string.mozac_feature_prompts_identity_credentials_choose_provider),
            style = TextStyle(
                fontSize = 16.sp,
                lineHeight = 24.sp,
                color = colors.title,
                letterSpacing = 0.15.sp,
                fontWeight = FontWeight.Bold,
            ),
            modifier = Modifier.padding(16.dp),
        )

        providers.forEach { provider ->
            ProviderItem(provider = provider, onClick = onProviderClick, colors = colors)
        }

        Spacer(modifier = Modifier.height(16.dp))
    }
}

@Composable
private fun ProviderItem(
    provider: Provider,
    modifier: Modifier = Modifier,
    colors: DialogColors = DialogColors.default(),
    onClick: (Provider) -> Unit,
) {
    IdentityCredentialItem(
        title = provider.name,
        description = provider.domain,
        modifier = modifier,
        colors = colors,
        onClick = { onClick(provider) },
    ) {
        provider.icon?.base64ToBitmap()?.asImageBitmap()?.let { bitmap ->
            Image(
                bitmap = bitmap,
                contentDescription = null,
                contentScale = ContentScale.FillWidth,
                modifier = Modifier
                    .padding(horizontal = 16.dp)
                    .size(24.dp),
            )
        } ?: Spacer(
            Modifier
                .padding(horizontal = 16.dp)
                .width(24.dp),
        )
    }
}

@Composable
@Preview(name = "Provider with no favicon")
private fun ProviderItemPreview() {
    ProviderItem(
        modifier = Modifier.background(Color.White),
        provider = Provider(
            0,
            null,
            "Title",
            "Description",
        ),
        onClick = {},
    )
}

@Composable
@LightDarkPreview
private fun SelectProviderDialogPreview() {
    DialogPreviewMaterialTheme {
        SelectProviderDialog(
            providers = listOf(
                Provider(
                    0,
                    null,
                    "Title",
                    "Description",
                ),
                Provider(
                    0,
                    GOOGLE_FAVICON,
                    "Google",
                    "google.com",
                ),
            ),
            modifier = Modifier.background(MaterialTheme.colors.background),
        ) { }
    }
}

@Suppress("MaxLineLength")
private const val GOOGLE_FAVICON =
    "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAARCAYAAADUryzEAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAH+SURBVHgBhVO/b9NQEL5z7Bi1lWJUCYRYHBaWDh6SKhspAYnRoVO3SqxIbiUGEEIRQ1kbxB9AM7VISJiNoSUeI6DCYmSpt1ZUak2U0GLXuZ7dOnUsp73l+e593/343hkhZYePKqp/EhhAoLOrRkEEmwgd/mrd3PpmJvGYdPbvl1cJYQkuMQI085KwfP1Lxwl9Mb74Uyu/J4BFuMIEoKp/HCixHyXYf1BqEF2QuS13gPQ2P1OyQt//ta0CYgOBFApg7ob13R5ij9rX1P67uzuDv/k45ASBMHfLOmsxabvVipqOo78pNZls/Nu8Df7vAgRBrphFHmfhCPeEggdT8zvg2dNrk8/2Rsi1N12dx1OyyIjghgnUOCBrB5/TIAJhNYkZuSNwBT4vsiNlVrrEFJHf3UE6q7DtTfO5N4Jg5W3u1Um0pPBza+eeE4nIH8aHozvQ7M+4J3JQtOumO65kbaU33BfWwOK9IPN5dzYkRy1JntAYR3640tOSy8YatKJVLm3Mt/moJrAWEL7+sfDRCp3qJ13peYIxsft0SezPxjo5X19OFaMEGgOk/7mflK12OM5QXPlAB/mwDjjA+tarSXP4M1XWdXVCLrS7Xi8ryUhCsV9a7jx5sRbpkL4trz9eJBQMnlBLExncypHU7CxsOHEQx5WJ5j4WoyQiiE6SlLRTu5e/Z+DrlXsAAAAASUVORK5CYII="
