/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

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
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.mozilla.experiments.nimbus.StringHolder
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.gleanplumb.Message
import org.mozilla.fenix.nimbus.MessageData
import org.mozilla.fenix.nimbus.StyleData
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

/**
 * Message Card.
 *
 * @param message [Message] that holds a representation of GleanPlum message from Nimbus.
 * @param onClick Invoked when user clicks on the message card.
 * @param onCloseButtonClick Invoked when user clicks on close button to remove message.
 */
@Suppress("LongMethod")
@Composable
fun MessageCard(
    message: Message,
    onClick: () -> Unit,
    onCloseButtonClick: () -> Unit,
) {
    Card(
        modifier = Modifier
            .padding(vertical = 16.dp)
            .then(
                if (message.data.buttonLabel.isNullOrBlank()) {
                    Modifier.clickable(onClick = onClick)
                } else {
                    Modifier
                }
            ),
        shape = RoundedCornerShape(8.dp),
        backgroundColor = FirefoxTheme.colors.layer2,
    ) {
        Column(
            Modifier
                .padding(all = 16.dp)
                .fillMaxWidth()
        ) {
            val title = message.data.title
            if (!title.isNullOrBlank()) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    SectionHeader(
                        text = title,
                        modifier = Modifier
                            .weight(1f)
                    )

                    IconButton(
                        modifier = Modifier.size(20.dp),
                        onClick = onCloseButtonClick
                    ) {
                        Icon(
                            painter = painterResource(R.drawable.mozac_ic_close_20),
                            contentDescription = stringResource(
                                R.string.content_description_close_button
                            ),
                            tint = FirefoxTheme.colors.iconPrimary
                        )
                    }
                }

                Text(
                    text = message.data.text,
                    modifier = Modifier.fillMaxWidth(),
                    fontSize = 14.sp
                )
            } else {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Text(
                        text = message.data.text,
                        modifier = Modifier.weight(1f),
                        fontSize = 14.sp
                    )

                    IconButton(
                        modifier = Modifier.size(20.dp),
                        onClick = onCloseButtonClick
                    ) {
                        Icon(
                            painter = painterResource(R.drawable.mozac_ic_close_20),
                            contentDescription = stringResource(
                                R.string.content_description_close_button
                            ),
                            tint = FirefoxTheme.colors.iconPrimary
                        )
                    }
                }
            }

            if (!message.data.buttonLabel.isNullOrBlank()) {
                Spacer(modifier = Modifier.height(16.dp))

                PrimaryButton(
                    text = stringResource(R.string.preferences_set_as_default_browser),
                    onClick = onClick
                )
            }
        }
    }
}

@Composable
@Preview
private fun MessageCardPreview() {
    FirefoxTheme(theme = Theme.getTheme(isPrivate = false)) {
        Box(Modifier.background(FirefoxTheme.colors.layer1)) {
            MessageCard(
                message = Message(
                    id = "end-",
                    data = MessageData(
                        title = StringHolder(
                            R.string.bookmark_empty_title_error,
                            "Title"
                        ),
                        text = StringHolder(
                            R.string.default_browser_experiment_card_text, "description"
                        )
                    ),
                    action = "action",
                    style = StyleData(),
                    triggers = listOf("trigger"),
                    metadata = Message.Metadata("end-")
                ),
                onClick = {},
                onCloseButtonClick = {}
            )
        }
    }
}

@Composable
@Preview
private fun MessageCardWithoutTitlePreview() {
    FirefoxTheme(theme = Theme.getTheme(isPrivate = false)) {
        Box(Modifier.background(FirefoxTheme.colors.layer1)) {
            MessageCard(
                message = Message(
                    id = "end-",
                    data = MessageData(
                        text = StringHolder(
                            R.string.default_browser_experiment_card_text, "description"
                        )
                    ),
                    action = "action",
                    style = StyleData(),
                    triggers = listOf("trigger"),
                    metadata = Message.Metadata("end-")
                ),
                onClick = {},
                onCloseButtonClick = {}
            )
        }
    }
}

@Composable
@Preview
private fun MessageCardWithButtonLabelPreview() {
    FirefoxTheme(theme = Theme.getTheme(isPrivate = false)) {
        Box(Modifier.background(FirefoxTheme.colors.layer1)) {
            MessageCard(
                message = Message(
                    id = "end-",
                    data = MessageData(
                        buttonLabel = StringHolder(R.string.preferences_set_as_default_browser, ""),
                        title = StringHolder(
                            R.string.bookmark_empty_title_error,
                            "Title"
                        ),
                        text = StringHolder(
                            R.string.default_browser_experiment_card_text, "description"
                        )
                    ),
                    action = "action",
                    style = StyleData(),
                    triggers = listOf("trigger"),
                    metadata = Message.Metadata("end-")
                ),
                onClick = {},
                onCloseButtonClick = {}
            )
        }
    }
}
