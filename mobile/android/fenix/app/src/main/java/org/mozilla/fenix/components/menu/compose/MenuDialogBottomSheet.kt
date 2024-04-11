/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.compose

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.BottomSheetHandle
import org.mozilla.fenix.theme.FirefoxTheme

private const val BOTTOM_SHEET_HANDLE_WIDTH_PERCENT = 0.1f

/**
 * The menu dialog bottom sheet.
 *
 * @param onRequestDismiss Invoked when when accessibility services or UI automation requests
 * dismissal of the bottom sheet.
 * @param content The children composable to be laid out.
 */
@Composable
fun MenuDialogBottomSheet(
    onRequestDismiss: () -> Unit,
    content: @Composable () -> Unit,
) {
    Column(
        modifier = Modifier
            .background(
                color = FirefoxTheme.colors.layer3,
                shape = RoundedCornerShape(topStart = 16.dp, topEnd = 16.dp),
            ),
    ) {
        BottomSheetHandle(
            onRequestDismiss = onRequestDismiss,
            contentDescription = stringResource(id = R.string.a11y_action_label_collapse),
            modifier = Modifier
                .padding(top = 8.dp, bottom = 5.dp)
                .fillMaxWidth(BOTTOM_SHEET_HANDLE_WIDTH_PERCENT)
                .align(Alignment.CenterHorizontally),
            color = FirefoxTheme.colors.borderInverted,
        )

        content()
    }
}
