/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.microsurvey.ui

import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Button
import androidx.compose.material.ButtonDefaults
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.LinkText
import org.mozilla.fenix.compose.LinkTextState
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * The footer UI used for micro-survey.
 *
 * @param isSubmitted Whether the user has "Submitted" the survey or not.
 * @param isContentAnswerSelected Whether the user clicked on one of the answers or not.
 * @param onLinkClick Invoked when the link is clicked.
 * @param onButtonClick Invoked when the "Submit"/"Close" button is clicked.
 */
@Composable
fun MicroSurveyFooter(
    isSubmitted: Boolean,
    isContentAnswerSelected: Boolean,
    onLinkClick: () -> Unit,
    onButtonClick: () -> Unit,
) {
    val buttonText = if (isSubmitted) {
        stringResource(id = R.string.micro_survey_close_button_label)
    } else {
        stringResource(id = R.string.micro_survey_submit_button_label)
    }
    val buttonColor = if (isContentAnswerSelected) {
        FirefoxTheme.colors.actionPrimary
    } else {
        FirefoxTheme.colors.actionPrimaryDisabled
    }

    Row(
        verticalAlignment = Alignment.Bottom,
        modifier = Modifier.fillMaxWidth(),
    ) {
        LinkText(
            text = stringResource(id = R.string.about_privacy_notice),
            linkTextStates = listOf(
                LinkTextState(
                    text = stringResource(id = R.string.micro_survey_privacy_notice),
                    url = "",
                    onClick = {
                        onLinkClick()
                    },
                ),
            ),
            style = FirefoxTheme.typography.caption,
            linkTextDecoration = TextDecoration.Underline,
        )

        Spacer(modifier = Modifier.weight(1f))

        Button(
            onClick = { onButtonClick() },
            enabled = isContentAnswerSelected,
            shape = RoundedCornerShape(size = 4.dp),
            colors = ButtonDefaults.buttonColors(
                backgroundColor = buttonColor,
            ),
            contentPadding = PaddingValues(16.dp, 12.dp),
        ) {
            Text(
                text = buttonText,
                color = FirefoxTheme.colors.textActionPrimary,
                style = FirefoxTheme.typography.button,
            )
        }
    }
}

/**
 * The preview for micro-survey footer.
 */
@LightDarkPreview
@Composable
private fun ReviewQualityCheckFooterPreview() {
    FirefoxTheme {
        MicroSurveyFooter(
            isSubmitted = false,
            isContentAnswerSelected = false,
            onLinkClick = {},
            onButtonClick = {},
        )
    }
}
