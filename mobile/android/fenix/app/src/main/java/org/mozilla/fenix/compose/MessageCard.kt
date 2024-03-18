/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Message Card.
 *
 * @param messageText The message card's body text to be displayed.
 * @param titleText An optional title of message card. If the provided title text is blank or null,
 * the title will not be shown.
 * @param buttonText An optional button text of the message card. If the provided button text is blank or null,
 * the button won't be shown.
 * @param messageColors The color set defined by [MessageCardColors] used to style the message card.
 * @param onClick Invoked when user clicks on the message card.
 * @param onCloseButtonClick Invoked when user clicks on close button to remove message.
 */
@Suppress("LongMethod")
@Composable
fun MessageCard(
    messageText: String,
    titleText: String? = null,
    buttonText: String? = null,
    messageColors: MessageCardColors = MessageCardColors.buildMessageCardColors(),
    onClick: () -> Unit,
    onCloseButtonClick: () -> Unit,
) {
    Card(
        modifier = Modifier
            .padding(vertical = 16.dp)
            .then(
                if (buttonText.isNullOrBlank()) {
                    Modifier.clickable(onClick = onClick)
                } else {
                    Modifier
                },
            ),
        shape = RoundedCornerShape(8.dp),
        backgroundColor = messageColors.backgroundColor,
    ) {
        Column(
            Modifier
                .padding(all = 16.dp)
                .fillMaxWidth(),
        ) {
            if (!titleText.isNullOrBlank()) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Text(
                        text = titleText,
                        modifier = Modifier.weight(1f),
                        color = messageColors.titleTextColor,
                        overflow = TextOverflow.Ellipsis,
                        maxLines = 2,
                        style = FirefoxTheme.typography.headline7,
                    )

                    MessageCardIconButton(
                        iconTint = messageColors.iconColor,
                        onCloseButtonClick = onCloseButtonClick,
                    )
                }

                Text(
                    text = messageText,
                    modifier = Modifier.fillMaxWidth(),
                    fontSize = 14.sp,
                    color = messageColors.messageTextColor,
                )
            } else {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Text(
                        text = messageText,
                        modifier = Modifier.weight(1f),
                        fontSize = 14.sp,
                        color = messageColors.titleTextColor,
                    )

                    MessageCardIconButton(
                        iconTint = messageColors.iconColor,
                        onCloseButtonClick = onCloseButtonClick,
                    )
                }
            }

            if (!buttonText.isNullOrBlank()) {
                Spacer(modifier = Modifier.height(16.dp))

                PrimaryButton(
                    text = buttonText,
                    textColor = messageColors.buttonTextColor,
                    backgroundColor = messageColors.buttonColor,
                    onClick = onClick,
                )
            }
        }
    }
}

/**
 * IconButton within a MessageCard.
 *
 * @param iconTint The [Color] used to tint the button's icon.
 * @param onCloseButtonClick Invoked when user clicks on close button to remove message.
 */
@Composable
private fun MessageCardIconButton(
    iconTint: Color,
    onCloseButtonClick: () -> Unit,
) {
    IconButton(
        modifier = Modifier.size(20.dp),
        onClick = onCloseButtonClick,
    ) {
        Icon(
            painter = painterResource(R.drawable.mozac_ic_cross_20),
            contentDescription = stringResource(
                R.string.content_description_close_button,
            ),
            tint = iconTint,
        )
    }
}

/**
 * Wrapper for the color parameters of [MessageCard].
 *
 * @property backgroundColor The background [Color] of the message.
 * @property titleTextColor [Color] to apply to the message's title, or the body text when there is no title.
 * @property messageTextColor [Color] to apply to the message's body text.
 * @property iconColor [Color] to apply to the message's icon.
 * @property buttonColor The background [Color] of the message's button.
 * @property buttonTextColor [Color] to apply to the button text.
 */
data class MessageCardColors(
    val backgroundColor: Color,
    val titleTextColor: Color,
    val messageTextColor: Color,
    val iconColor: Color,
    val buttonColor: Color,
    val buttonTextColor: Color,
) {
    companion object {

        /**
         * Builder function used to construct an instance of [MessageCardColors].
         */
        @Composable
        fun buildMessageCardColors(
            backgroundColor: Color = FirefoxTheme.colors.layer2,
            titleTextColor: Color = FirefoxTheme.colors.textPrimary,
            messageTextColor: Color = FirefoxTheme.colors.textSecondary,
            iconColor: Color = FirefoxTheme.colors.iconPrimary,
            buttonColor: Color = FirefoxTheme.colors.actionPrimary,
            buttonTextColor: Color = FirefoxTheme.colors.textActionPrimary,
        ): MessageCardColors =
            MessageCardColors(
                backgroundColor = backgroundColor,
                titleTextColor = titleTextColor,
                messageTextColor = messageTextColor,
                iconColor = iconColor,
                buttonColor = buttonColor,
                buttonTextColor = buttonTextColor,
            )
    }
}

@Composable
@LightDarkPreview
private fun MessageCardPreview() {
    FirefoxTheme {
        Box(
            Modifier
                .background(FirefoxTheme.colors.layer1)
                .padding(all = 16.dp),
        ) {
            MessageCard(
                messageText = stringResource(id = R.string.default_browser_experiment_card_text),
                titleText = stringResource(id = R.string.default_browser_experiment_card_title),
                onClick = {},
                onCloseButtonClick = {},
            )
        }
    }
}

@Composable
@LightDarkPreview
private fun MessageCardWithoutTitlePreview() {
    FirefoxTheme {
        Box(
            modifier = Modifier
                .background(FirefoxTheme.colors.layer1)
                .padding(all = 16.dp),
        ) {
            MessageCard(
                messageText = stringResource(id = R.string.default_browser_experiment_card_text),
                onClick = {},
                onCloseButtonClick = {},
            )
        }
    }
}

@Composable
@LightDarkPreview
private fun MessageCardWithButtonLabelPreview() {
    FirefoxTheme {
        Box(
            modifier = Modifier
                .background(FirefoxTheme.colors.layer1)
                .padding(all = 16.dp),
        ) {
            MessageCard(
                messageText = stringResource(id = R.string.default_browser_experiment_card_text),
                titleText = stringResource(id = R.string.default_browser_experiment_card_title),
                buttonText = stringResource(id = R.string.preferences_set_as_default_browser),
                onClick = {},
                onCloseButtonClick = {},
            )
        }
    }
}
