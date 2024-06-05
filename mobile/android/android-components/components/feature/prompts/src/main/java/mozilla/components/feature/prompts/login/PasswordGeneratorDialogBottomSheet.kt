/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.login

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Button
import androidx.compose.material.ButtonDefaults
import androidx.compose.material.Text
import androidx.compose.material.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ColorFilter
import androidx.compose.ui.graphics.RectangleShape
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.feature.prompts.R
import mozilla.components.feature.prompts.identitycredential.previews.DialogPreviewMaterialTheme

private val FONT_SIZE = 16.sp
private val LINE_HEIGHT = 24.sp
private val LETTER_SPACING = 0.15.sp

/**
 * The password generator bottom sheet
 *
 * @param generatedStrongPassword The generated password.
 * @param onUsePassword Invoked when the user clicks on the UsePassword button.
 * @param onCancelDialog Invoked when the user clicks on the NotNow button.
 * @param colors The colors of the dialog.
 */
@Composable
fun PasswordGeneratorBottomSheet(
    generatedStrongPassword: String,
    onUsePassword: () -> Unit,
    onCancelDialog: () -> Unit,
    colors: PasswordGeneratorDialogColors = PasswordGeneratorDialogColors.default(),
) {
    Column(
        modifier = Modifier
            .background(colors.background)
            .padding(all = 8.dp)
            .fillMaxWidth(),
        verticalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        StrongPasswordBottomSheetTitle(colors = colors)

        StrongPasswordBottomSheetDescription(colors = colors)

        StrongPasswordBottomSheetPasswordBox(
            generatedPassword = generatedStrongPassword,
            colors = colors,
        )

        StrongPasswordBottomSheetButtons(
            onUsePassword = { onUsePassword() },
            onCancelDialog = { onCancelDialog() },
            colors = colors,
        )
    }
}

@Composable
private fun StrongPasswordBottomSheetTitle(colors: PasswordGeneratorDialogColors) {
    Row {
        Image(
            painter = painterResource(id = R.drawable.mozac_ic_login_24),
            contentDescription = null,
            contentScale = ContentScale.FillWidth,
            colorFilter = ColorFilter.tint(colors.title),
            modifier = Modifier
                .align(Alignment.CenterVertically),
        )

        Text(
            modifier = Modifier.padding(16.dp),
            text = stringResource(id = R.string.mozac_feature_prompts_suggest_strong_password_title),
            style = TextStyle(
                fontSize = FONT_SIZE,
                lineHeight = LINE_HEIGHT,
                color = colors.title,
                letterSpacing = LETTER_SPACING,
                fontWeight = FontWeight.Bold,
            ),
        )
    }
}

@Composable
private fun StrongPasswordBottomSheetDescription(
    modifier: Modifier = Modifier,
    colors: PasswordGeneratorDialogColors,
) {
    Text(
        modifier = modifier.padding(start = 40.dp, top = 0.dp, end = 12.dp, bottom = 16.dp),
        text = stringResource(id = R.string.mozac_feature_prompts_suggest_strong_password_description),
        style = TextStyle(
            fontSize = FONT_SIZE,
            lineHeight = LINE_HEIGHT,
            color = colors.description,
            letterSpacing = LETTER_SPACING,
        ),
    )
}

@Composable
private fun StrongPasswordBottomSheetPasswordBox(
    modifier: Modifier = Modifier,
    generatedPassword: String,
    colors: PasswordGeneratorDialogColors,
) {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .padding(start = 40.dp, top = 8.dp, end = 12.dp, bottom = 8.dp)
            .background(colors.passwordBox)
            .border(1.dp, colors.boxBorder)
            .padding(4.dp),
    ) {
        Text(
            modifier = modifier.padding(8.dp),
            text = generatedPassword,
            style = TextStyle(
                fontSize = FONT_SIZE,
                lineHeight = LINE_HEIGHT,
                color = colors.title,
                letterSpacing = LETTER_SPACING,
            ),
        )
    }
}

@Composable
private fun StrongPasswordBottomSheetButtons(
    onUsePassword: () -> Unit,
    onCancelDialog: () -> Unit,
    colors: PasswordGeneratorDialogColors,
) {
    Row(
        horizontalArrangement = Arrangement.spacedBy(4.dp, Alignment.End),
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 12.dp, vertical = 16.dp)
            .height(48.dp),
    ) {
        TextButton(
            onClick = { onCancelDialog() },
            shape = RectangleShape,
            colors = ButtonDefaults.buttonColors(backgroundColor = colors.background),
            modifier = Modifier.height(48.dp),
        ) {
            Text(
                text = stringResource(id = R.string.mozac_feature_prompt_not_now),
                style = TextStyle(
                    fontSize = 16.sp,
                    lineHeight = 24.sp,
                    color = colors.confirmButton,
                    letterSpacing = 0.15.sp,
                    fontWeight = FontWeight.Bold,
                ),
            )
        }

        Button(
            onClick = { onUsePassword() },
            shape = RectangleShape,
            colors = ButtonDefaults.buttonColors(backgroundColor = colors.confirmButton),
            modifier = Modifier.height(48.dp),
        ) {
            Text(
                text = stringResource(id = R.string.mozac_feature_prompts_suggest_strong_password_use_password),
                style = TextStyle(
                    fontSize = 16.sp,
                    lineHeight = 24.sp,
                    color = Color.White,
                    letterSpacing = 0.15.sp,
                    fontWeight = FontWeight.Bold,
                ),
            )
        }
    }
}

@Composable
@Preview
private fun GenerateStrongPasswordDialogPreview() {
    DialogPreviewMaterialTheme {
        PasswordGeneratorBottomSheet(
            generatedStrongPassword = "StrongPassword123#",
            onUsePassword = {},
            onCancelDialog = {},
        )
    }
}
