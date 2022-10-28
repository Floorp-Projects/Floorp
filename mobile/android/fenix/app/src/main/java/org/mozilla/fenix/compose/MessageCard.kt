/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import android.content.res.Configuration
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Message Card.
 *
 * @param messageText The message card's body text to be displayed.
 * @param titleText An optional title of message card. If the title is blank or null is provided,
 * the title will not be shown.
 * @param buttonText An optional button text of the message card. If the button text is blank or null is provided,
 * the button won't be shown.
 * @param onClick Invoked when user clicks on the message card.
 * @param onCloseButtonClick Invoked when user clicks on close button to remove message.
 */
@Suppress("LongMethod")
@Composable
fun MessageCard(
    messageText: String,
    titleText: String? = null,
    buttonText: String? = null,
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
        backgroundColor = FirefoxTheme.colors.layer2,
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
                        color = FirefoxTheme.colors.textPrimary,
                        overflow = TextOverflow.Ellipsis,
                        maxLines = 2,
                        style = FirefoxTheme.typography.headline7,
                    )

                    IconButton(
                        modifier = Modifier.size(20.dp),
                        onClick = onCloseButtonClick,
                    ) {
                        Icon(
                            painter = painterResource(R.drawable.mozac_ic_close_20),
                            contentDescription = stringResource(
                                R.string.content_description_close_button,
                            ),
                            tint = FirefoxTheme.colors.iconPrimary,
                        )
                    }
                }

                Text(
                    text = messageText,
                    modifier = Modifier.fillMaxWidth(),
                    fontSize = 14.sp,
                    color = FirefoxTheme.colors.textSecondary,
                )
            } else {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Text(
                        text = messageText,
                        modifier = Modifier.weight(1f),
                        fontSize = 14.sp,
                        color = FirefoxTheme.colors.textPrimary,
                    )

                    IconButton(
                        modifier = Modifier.size(20.dp),
                        onClick = onCloseButtonClick,
                    ) {
                        Icon(
                            painter = painterResource(R.drawable.mozac_ic_close_20),
                            contentDescription = stringResource(
                                R.string.content_description_close_button,
                            ),
                            tint = FirefoxTheme.colors.iconPrimary,
                        )
                    }
                }
            }

            if (!buttonText.isNullOrBlank()) {
                Spacer(modifier = Modifier.height(16.dp))

                PrimaryButton(
                    text = buttonText,
                    onClick = onClick,
                )
            }
        }
    }
}

@Composable
@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES)
@Preview(uiMode = Configuration.UI_MODE_NIGHT_NO)
private fun MessageCardPreview() {
    FirefoxTheme {
        Box(
            Modifier
                .background(FirefoxTheme.colors.layer1)
                .padding(all = 16.dp),
        ) {
            MessageCard(
                messageText = stringResource(id = R.string.default_browser_experiment_card_text),
                titleText = stringResource(id = R.string.bookmark_empty_title_error),
                onClick = {},
                onCloseButtonClick = {},
            )
        }
    }
}

@Composable
@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES)
@Preview(uiMode = Configuration.UI_MODE_NIGHT_NO)
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
@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES)
@Preview(uiMode = Configuration.UI_MODE_NIGHT_NO)
private fun MessageCardWithButtonLabelPreview() {
    FirefoxTheme {
        Box(
            modifier = Modifier
                .background(FirefoxTheme.colors.layer1)
                .padding(all = 16.dp),
        ) {
            MessageCard(
                messageText = stringResource(id = R.string.default_browser_experiment_card_text),
                titleText = stringResource(id = R.string.bookmark_empty_title_error),
                buttonText = stringResource(id = R.string.preferences_set_as_default_browser),
                onClick = {},
                onCloseButtonClick = {},
            )
        }
    }
}
