/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
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

private val cardShape = RoundedCornerShape(8.dp)

/**
 * Review Quality Check info UI.
 *
 * @param title The primary text of the info message.
 * @param type The [ReviewQualityCheckInfoType] of message to display.
 * @param modifier Modifier to be applied to the card.
 * @param description The optional secondary piece of text.
 * @param linkText An optional piece of text with a clickable substring.
 * @param hyperlinkText The text within [linkText] that is clickable.
 * @param linkURL The optional URL to return when the link is clicked.
 * @param buttonText The text to show in the optional button.
 * @param onLinkClick Invoked when the link is clicked. When not-null, the link will be visible.
 * @param onButtonClick Invoked when the button is clicked. When not-null, the button will be visible.
 * @param icon The UI to display before the text, typically an image or loading spinner.
 */
@Composable
fun ReviewQualityCheckInfoCard(
    title: String,
    type: ReviewQualityCheckInfoType,
    modifier: Modifier = Modifier,
    description: String? = null,
    linkText: String = "",
    hyperlinkText: String = "",
    linkURL: String = "",
    buttonText: String = "",
    onLinkClick: (() -> Unit)? = null,
    onButtonClick: (() -> Unit)? = null,
    icon: @Composable () -> Unit,
) {
    Card(
        modifier = modifier,
        shape = cardShape,
        backgroundColor = type.cardBackgroundColor,
        elevation = 0.dp,
    ) {
        Column(modifier = Modifier.padding(horizontal = 12.dp, vertical = 8.dp)) {
            Row {
                icon()

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

                    onLinkClick?.let {
                        Spacer(modifier = Modifier.height(4.dp))

                        LinkText(
                            text = linkText,
                            linkTextState = LinkTextState(
                                text = hyperlinkText,
                                url = linkURL,
                                onClick = { onLinkClick() },
                            ),
                            style = FirefoxTheme.typography.body2.copy(
                                color = FirefoxTheme.colors.textPrimary,
                            ),
                            linkTextColor = FirefoxTheme.colors.textPrimary,
                            linkTextDecoration = TextDecoration.Underline,
                        )
                    }
                }
            }

            onButtonClick?.let {
                Spacer(modifier = Modifier.height(8.dp))

                PrimaryButton(
                    text = buttonText,
                    textColor = type.buttonTextColor,
                    backgroundColor = type.buttonBackgroundColor,
                    onClick = onButtonClick,
                )
            }
        }
    }
}

/**
 * The possible types of a [ReviewQualityCheckInfoCard].
 */
enum class ReviewQualityCheckInfoType {

    Warning,
    Confirmation,
    Error,
    Info,
    ;

    val cardBackgroundColor: Color
        @Composable
        get() = when (this) {
            Warning -> FirefoxTheme.colors.layerWarning
            Confirmation -> FirefoxTheme.colors.layerConfirmation
            Error -> FirefoxTheme.colors.layerError
            Info -> FirefoxTheme.colors.layerInfo
        }

    val buttonBackgroundColor: Color
        @Composable
        get() = when (this) {
            Warning -> FirefoxTheme.colors.actionWarning
            Confirmation -> FirefoxTheme.colors.actionConfirmation
            Error -> FirefoxTheme.colors.actionError
            Info -> FirefoxTheme.colors.actionInfo
        }

    val buttonTextColor: Color
        @Composable
        get() = when {
            this == Info && !isSystemInDarkTheme() -> FirefoxTheme.colors.textOnColorPrimary
            else -> FirefoxTheme.colors.textPrimary
        }
}

private class PreviewModel(
    val messageType: ReviewQualityCheckInfoType,
    val iconId: Int,
)

private class PreviewModelParameterProvider : PreviewParameterProvider<PreviewModel> {
    override val values: Sequence<PreviewModel>
        get() = sequenceOf(
            PreviewModel(ReviewQualityCheckInfoType.Warning, R.drawable.mozac_ic_warning_fill_24),
            PreviewModel(ReviewQualityCheckInfoType.Confirmation, R.drawable.mozac_ic_checkmark_24),
            PreviewModel(ReviewQualityCheckInfoType.Error, R.drawable.mozac_ic_information_fill_24),
            PreviewModel(ReviewQualityCheckInfoType.Info, R.drawable.mozac_ic_information_fill_24),
        )
}

@LightDarkPreview
@Composable
private fun InfoCardPreview(
    @PreviewParameter(PreviewModelParameterProvider::class) model: PreviewModel,
) {
    FirefoxTheme {
        Box(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer1)
                .padding(16.dp),
        ) {
            ReviewQualityCheckInfoCard(
                title = "Title text",
                type = model.messageType,
                modifier = Modifier.fillMaxWidth(),
                description = "Description text",
                linkText = "Primary link text with an underlined hyperlink.",
                hyperlinkText = "underlined hyperlink",
                buttonText = "Button text",
                onLinkClick = {},
                onButtonClick = {},
                icon = {
                    Icon(
                        painter = painterResource(id = model.iconId),
                        contentDescription = null,
                        modifier = Modifier.size(24.dp),
                        tint = FirefoxTheme.colors.iconPrimary,
                    )
                },
            )
        }
    }
}
