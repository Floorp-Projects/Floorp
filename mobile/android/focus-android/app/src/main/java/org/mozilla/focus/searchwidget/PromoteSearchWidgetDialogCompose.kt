/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.searchwidget

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.foundation.layout.wrapContentSize
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Button
import androidx.compose.material.ButtonDefaults
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.colorResource
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import androidx.constraintlayout.compose.ConstraintLayout
import mozilla.components.ui.colors.PhotonColors
import org.mozilla.focus.R
import org.mozilla.focus.ui.theme.FocusTheme
import org.mozilla.focus.ui.theme.focusColors
import org.mozilla.focus.ui.theme.focusTypography

@Composable
@Preview
private fun PromoteSearchWidgetDialogComposePreview() {
    FocusTheme {
        PromoteSearchWidgetDialogCompose({}, {})
    }
}

@Suppress("LongMethod")
@Composable
fun PromoteSearchWidgetDialogCompose(
    onAddSearchWidgetButtonClick: () -> Unit,
    onDismiss: () -> Unit,
) {
    val openDialog = remember { mutableStateOf(true) }
    if (openDialog.value) {
        Dialog(
            onDismissRequest = {
                onDismiss()
            },
            DialogProperties(dismissOnBackPress = true, dismissOnClickOutside = false),
        ) {
            Column(
                modifier = Modifier
                    .clip(RoundedCornerShape(20.dp)),
            ) {
                ConstraintLayout(
                    modifier = Modifier
                        .wrapContentSize()
                        .background(
                            colorResource(id = R.color.promote_search_widget_dialog_background),
                        ),
                ) {
                    val (closeButton, content) = createRefs()
                    Column(
                        modifier = Modifier
                            .fillMaxWidth()
                            .wrapContentHeight()
                            .padding(top = 8.dp, end = 16.dp)
                            .constrainAs(closeButton) {
                                top.linkTo(parent.top)
                                start.linkTo(parent.start)
                                end.linkTo(parent.end)
                            },
                        verticalArrangement = Arrangement.Center,
                        horizontalAlignment = Alignment.End,
                    ) {
                        CloseButton(openDialog, onDismiss)
                    }
                    Column(
                        modifier = Modifier.constrainAs(content) {
                            top.linkTo(closeButton.bottom)
                            start.linkTo(parent.start)
                            end.linkTo(parent.end)
                        },
                        verticalArrangement = Arrangement.Center,
                        horizontalAlignment = Alignment.CenterHorizontally,
                    ) {
                        Text(
                            text = stringResource(
                                id = R.string.promote_search_widget_dialog_title,
                            ),
                            modifier = Modifier
                                .padding(16.dp),
                            color = focusColors.dialogTextColor,
                            textAlign = TextAlign.Center,
                            style = focusTypography.dialogTitle,
                        )
                        Text(
                            text = stringResource(
                                id = R.string.promote_search_widget_dialog_subtitle,
                                LocalContext.current.getString(R.string.onboarding_short_app_name),
                            ),
                            modifier = Modifier
                                .padding(top = 16.dp, start = 16.dp, end = 16.dp),
                            color = focusColors.dialogTextColor,
                            textAlign = TextAlign.Center,
                            style = focusTypography.dialogContent,
                        )
                        Image(
                            painter = painterResource(R.drawable.focus_search_widget_promote_dialog),
                            contentDescription = LocalContext.current.getString(
                                R.string.promote_search_widget_dialog_picture_content_description,
                            ),
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(start = 10.dp, end = 10.dp)
                                .background(
                                    colorResource(id = R.color.promote_search_widget_dialog_background),
                                ),
                        )
                        ComponentAddWidgetButton({ onAddSearchWidgetButtonClick() }, { onDismiss() }, openDialog)
                    }
                }
            }
        }
    }
}

@Composable
private fun ComponentAddWidgetButton(
    onAddSearchWidgetButtonClick: () -> Unit,
    onDismiss: () -> Unit,
    openState: MutableState<Boolean>,
) {
    Button(
        onClick = {
            openState.value = false
            onAddSearchWidgetButtonClick()
            onDismiss()
        },
        modifier = Modifier
            .fillMaxWidth()
            .padding(24.dp),
        shape = RoundedCornerShape(8.dp),
        colors = ButtonDefaults.textButtonColors(
            backgroundColor = colorResource(R.color.promote_search_widget_dialog_add_widget_button_background),
        ),
    ) {
        Text(
            text = AnnotatedString(
                LocalContext.current.resources.getString(
                    R.string.promote_search_widget_button_text,
                ),
            ),
            color = PhotonColors.White,
        )
    }
}

@Composable
private fun CloseButton(
    openState: MutableState<Boolean>,
    onDismiss: () -> Unit,
) {
    IconButton(
        onClick = {
            onDismiss()
            openState.value = false
        },
        modifier = Modifier
            .background(
                colorResource(id = R.color.promote_search_widget_dialog_close_button_background),
                shape = CircleShape,
            )
            .size(48.dp)
            .padding(10.dp),
    ) {
        Icon(
            painter = painterResource(R.drawable.mozac_ic_cross_24),
            contentDescription = stringResource(id = R.string.promote_search_widget_dialog_content_description),
            tint = focusColors.closeIcon,
        )
    }
}
