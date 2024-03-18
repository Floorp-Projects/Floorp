/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.ui.dialog

import android.content.res.Configuration
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.material.AlertDialog
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Text
import androidx.compose.material.TextButton
import androidx.compose.material.TextField
import androidx.compose.material.TextFieldDefaults
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import org.mozilla.focus.ui.theme.FocusTheme
import org.mozilla.focus.ui.theme.focusColors
import org.mozilla.focus.ui.theme.focusTypography

/**
 * Reusable composable for an alert dialog.
 *
 * @param dialogTitle (mandatory) text displayed as the dialog title.
 * @param dialogTextComposable (optional) A composable displayed as dialog text
 * @param dialogText (optional) A text displayed as dialog text when [dialogTextComposable] is null.
 * @param onConfirmRequest (event) Action to perform on dialog confirm action.
 * @param onDismissRequest (event) Action to perform on dialog dismissal.
 * @param isConfirmButtonEnabled (optional) whether the confirm [DialogTextButton] is enabled. Default value is true.
 * @param isDismissButtonEnabled (optional) whether the confirm [DialogTextButton] is enabled. Default value is true.
 * @param isConfirmButtonVisible (optional) whether the confirm [DialogTextButton] is visible. Default value is true.
 * @param isDismissButtonVisible (optional)whether the confirm [DialogTextButton] is visible. Default value is true.
 *
 */
@Suppress("LongParameterList")
@Composable
fun FocusDialog(
    dialogTitle: String,
    dialogTextComposable: @Composable (() -> Unit)? = null,
    dialogText: String = "",
    onConfirmRequest: () -> Unit,
    onDismissRequest: () -> Unit,
    confirmButtonText: String = "null",
    dismissButtonText: String = "null",
    isConfirmButtonEnabled: Boolean = true,
    isDismissButtonEnabled: Boolean = true,
    isConfirmButtonVisible: Boolean = true,
    isDismissButtonVisible: Boolean = true,
) {
    AlertDialog(
        onDismissRequest = onDismissRequest,
        title = { DialogTitle(text = dialogTitle) },
        text = dialogTextComposable ?: { DialogText(text = dialogText) },
        confirmButton = {
            if (isConfirmButtonVisible) {
                DialogTextButton(
                    text = confirmButtonText,
                    onClick = onConfirmRequest,
                    enabled = isConfirmButtonEnabled,
                )
            }
        },
        dismissButton = {
            if (isDismissButtonVisible) {
                DialogTextButton(
                    text = dismissButtonText,
                    onClick = onDismissRequest,
                    enabled = isDismissButtonEnabled,
                )
            }
        },
        backgroundColor = focusColors.secondary,
        shape = MaterialTheme.shapes.medium,
    )
}

/**
 * Reusable composable for a dialog title.
 */
@Composable
fun DialogTitle(
    modifier: Modifier = Modifier,
    text: String,
) {
    Text(
        modifier = modifier,
        color = focusColors.onPrimary,
        text = text,
        style = focusTypography.dialogTitle,
    )
}

/**
 * Reusable composable for a dialog text.
 */
@Composable
fun DialogText(
    modifier: Modifier = Modifier,
    text: String,
) {
    Text(
        modifier = modifier,
        color = focusColors.onPrimary,
        text = text,
        style = focusTypography.dialogInput,
    )
}

/**
 * Reusable composable for a dialog button with text.
 */
@Composable
fun DialogTextButton(
    text: String,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    onClick: () -> Unit,
) {
    TextButton(
        onClick = onClick,
        modifier = modifier,
        enabled = enabled,
        shape = MaterialTheme.shapes.large,
    ) {
        Text(
            modifier = modifier,
            color = if (enabled) {
                focusColors.dialogActiveControls
            } else {
                focusColors.dialogActiveControls.copy(alpha = 0.5f)
            },
            text = text,
            style = MaterialTheme.typography.button,
        )
    }
}

/**
 * Reusable composable for a dialog input field.
 */
@Composable
fun DialogInputField(
    modifier: Modifier = Modifier,
    text: String,
    placeholder: @Composable () -> Unit,
    onValueChange: (String) -> Unit,
) {
    TextField(
        modifier = modifier
            .wrapContentHeight(),
        value = text,
        placeholder = placeholder,
        onValueChange = onValueChange,
        textStyle = focusTypography.dialogInput,
        colors = TextFieldDefaults.textFieldColors(
            backgroundColor = focusColors.secondary,
            textColor = focusColors.onSecondary,
            cursorColor = focusColors.onPrimary,
            focusedIndicatorColor = focusColors.dialogActiveControls,
            unfocusedIndicatorColor = focusColors.dialogActiveControls,
        ),
        singleLine = true,
        shape = MaterialTheme.shapes.large,
    )
}

@Preview(
    name = "dark theme",
    showBackground = true,
    backgroundColor = 0xFF393473,
    uiMode = Configuration.UI_MODE_NIGHT_MASK,
)
@Composable
fun DialogTitlePreviewDark() {
    FocusTheme {
        FocusDialogSample()
    }
}

@Preview(
    name = "light theme",
    showBackground = true,
    backgroundColor = 0xFFFBFBFE,
    uiMode = Configuration.UI_MODE_NIGHT_NO,
)
@Composable
fun DialogTitlePreviewLight() {
    FocusTheme {
        FocusDialogSample()
    }
}

@Composable
fun FocusDialogSample() {
    FocusDialog(
        dialogTitle = "Sample dialog",
        dialogTextComposable = null,
        dialogText = "Sample dialog text",
        onConfirmRequest = { },
        onDismissRequest = { },
        confirmButtonText = "OK",
        dismissButtonText = "CANCEL",
        isConfirmButtonEnabled = true,
        isDismissButtonEnabled = true,
        isConfirmButtonVisible = true,
        isDismissButtonVisible = true,
    )
}
