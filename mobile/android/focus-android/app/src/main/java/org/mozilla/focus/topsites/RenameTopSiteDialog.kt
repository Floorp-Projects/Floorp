/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.topsites

import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.res.stringResource
import org.mozilla.focus.R
import org.mozilla.focus.ui.dialog.DialogInputField
import org.mozilla.focus.ui.dialog.FocusDialog

/**
 * Display a dialog for renaming a top site
 *
 * @param currentName (event) Current top site name.
 * @param onConfirm (event) Perform top site rename.
 * @param onDismiss (event) Action to perform on dialog dismissal.
 */
@Composable
fun RenameTopSiteDialog(
    currentName: String,
    onConfirm: (String) -> Unit,
    onDismiss: () -> Unit,
) {
    var text by remember { mutableStateOf(currentName) }

    FocusDialog(
        dialogTitle = stringResource(R.string.rename_top_site),
        dialogTextComposable = {
            DialogInputField(
                text = text,
                placeholder = { Text(stringResource(id = R.string.placeholder_rename_top_site)) },
            ) { newText -> text = newText }
        },
        onDismissRequest = { onDismiss.invoke() },
        onConfirmRequest = { onConfirm.invoke(text) },
        confirmButtonText = stringResource(android.R.string.ok),
        dismissButtonText = stringResource(android.R.string.cancel),
        isConfirmButtonEnabled = text.isNotEmpty(),
    )
}
