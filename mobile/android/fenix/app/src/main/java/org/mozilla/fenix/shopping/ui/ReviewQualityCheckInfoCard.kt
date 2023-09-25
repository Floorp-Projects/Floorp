/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.tooling.preview.PreviewParameter
import androidx.compose.ui.tooling.preview.PreviewParameterProvider
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.LinkText
import org.mozilla.fenix.compose.LinkTextState
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Review Quality Check Info Card UI.
 *
 * @param title The primary text of the info message.
 * @param type The [ReviewQualityCheckInfoType] of message to display.
 * @param modifier Modifier to be applied to the card.
 * @param description The optional secondary piece of text.
 * @param footer An optional piece of text with a clickable link.
 * @param buttonText The text to show in the optional button.
 */
@Composable
fun ReviewQualityCheckInfoCard(
    title: String,
    type: ReviewQualityCheckInfoType,
    modifier: Modifier = Modifier,
    description: String? = null,
    footer: Pair<String, LinkTextState>? = null,
    buttonText: InfoCardButtonText? = null,
) {
    ReviewQualityCheckCard(
        modifier = modifier,
        backgroundColor = type.cardBackgroundColor,
        contentPadding = PaddingValues(
            horizontal = 12.dp,
            vertical = 8.dp,
        ),
        elevation = 0.dp,
    ) {
        Row {
            when (type) {
                ReviewQualityCheckInfoType.Warning -> {
                    InfoCardIcon(iconId = R.drawable.mozac_ic_warning_fill_24)
                }

                ReviewQualityCheckInfoType.Confirmation -> {
                    InfoCardIcon(iconId = R.drawable.mozac_ic_checkmark_24)
                }

                ReviewQualityCheckInfoType.Error,
                ReviewQualityCheckInfoType.Info,
                ReviewQualityCheckInfoType.AnalysisUpdate,
                -> {
                    InfoCardIcon(
                        iconId = R.drawable.mozac_ic_information_fill_24,
                        modifier = Modifier.size(24.dp),
                    )
                }

                ReviewQualityCheckInfoType.Loading -> {
                    IndeterminateProgressIndicator(modifier = Modifier.size(24.dp))
                }
            }

            Spacer(modifier = Modifier.width(12.dp))

            Column {
                Text(
                    text = title,
                    color = FirefoxTheme.colors.textPrimary,
                    style = FirefoxTheme.typography.headline8,
                )

                description?.let {
                    Spacer(modifier = Modifier.height(4.dp))

                    Text(
                        text = description,
                        color = FirefoxTheme.colors.textPrimary,
                        style = FirefoxTheme.typography.body2,
                    )
                }

                footer?.let {
                    Spacer(modifier = Modifier.height(4.dp))

                    LinkText(
                        text = it.first,
                        linkTextStates = listOf(it.second),
                        style = FirefoxTheme.typography.body2.copy(
                            color = FirefoxTheme.colors.textPrimary,
                        ),
                        linkTextColor = FirefoxTheme.colors.textPrimary,
                        linkTextDecoration = TextDecoration.Underline,
                    )
                }
            }
        }

        buttonText?.let {
            Spacer(modifier = Modifier.height(8.dp))

            PrimaryButton(
                text = it.text,
                textColor = type.buttonTextColor,
                backgroundColor = type.buttonBackgroundColor,
                onClick = it.onClick,
            )
        }
    }
}

@Composable
private fun InfoCardIcon(
    iconId: Int,
    modifier: Modifier = Modifier,
) {
    Icon(
        painter = painterResource(id = iconId),
        contentDescription = null,
        tint = FirefoxTheme.colors.iconPrimary,
        modifier = modifier,
    )
}

/**
 * The possible types of a [ReviewQualityCheckInfoCard].
 */
enum class ReviewQualityCheckInfoType {

    Warning,
    Confirmation,
    Error,
    Info,
    AnalysisUpdate,
    Loading,
    ;

    val cardBackgroundColor: Color
        @Composable
        get() = when (this) {
            Warning -> FirefoxTheme.colors.layerWarning
            Confirmation -> FirefoxTheme.colors.layerConfirmation
            Error -> FirefoxTheme.colors.layerError
            Info -> FirefoxTheme.colors.layerInfo
            AnalysisUpdate -> Color.Transparent
            Loading -> Color.Transparent
        }

    val buttonBackgroundColor: Color
        @Composable
        get() = when (this) {
            Warning -> FirefoxTheme.colors.actionWarning
            Confirmation -> FirefoxTheme.colors.actionConfirmation
            Error -> FirefoxTheme.colors.actionError
            Info -> FirefoxTheme.colors.actionInfo
            AnalysisUpdate -> FirefoxTheme.colors.actionSecondary
            Loading -> FirefoxTheme.colors.actionSecondary
        }

    val buttonTextColor: Color
        @Composable
        get() = when {
            this == Info && !isSystemInDarkTheme() -> FirefoxTheme.colors.textOnColorPrimary
            this == AnalysisUpdate || this == Loading -> FirefoxTheme.colors.textActionSecondary
            else -> FirefoxTheme.colors.textPrimary
        }
}

/**
 * Model for the optional button in a [ReviewQualityCheckInfoCard].
 *
 * @param text The text to show in the button.
 * @param onClick The callback to invoke when the button is clicked.
 */
data class InfoCardButtonText(
    val text: String,
    val onClick: () -> Unit,
)

private class PreviewModelParameterProvider : PreviewParameterProvider<ReviewQualityCheckInfoType> {

    override val values = enumValues<ReviewQualityCheckInfoType>().asSequence()
}

@LightDarkPreview
@Composable
private fun InfoCardPreview(
    @PreviewParameter(PreviewModelParameterProvider::class) type: ReviewQualityCheckInfoType,
) {
    FirefoxTheme {
        Box(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer1)
                .padding(16.dp),
        ) {
            ReviewQualityCheckInfoCard(
                title = "Title text",
                type = type,
                modifier = Modifier.fillMaxWidth(),
                description = "Description text",
                footer = "Primary link text with an underlined hyperlink." to LinkTextState(
                    text = "underlined hyperlink",
                    url = "https://www.mozilla.org",
                    onClick = {},
                ),
                buttonText = InfoCardButtonText(
                    text = "Button text",
                    onClick = {},
                ),
            )
        }
    }
}
