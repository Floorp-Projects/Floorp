/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.microsurvey.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * The UI used for micro-survey when this is opened.
 *
 * @param isSubmitted Whether the user submitted an answer or not.
 * @param isContentAnswerSelected Whether the user clicked on one of the answers or not.
 * @param content The content of the micro-survey.
 */
@Composable
fun MicroSurveyOpenView(
    isSubmitted: Boolean,
    isContentAnswerSelected: Boolean,
    content: @Composable () -> Unit,
) {
    Column(
        modifier = Modifier.padding(horizontal = 16.dp),
    ) {
        Row(
            modifier = Modifier.padding(
                top = 20.dp,
                bottom = 10.dp,
            ),
        ) {
            MicroSurveyHeader(
                stringResource(id = R.string.micro_survey_survey_header),
            ) {}
        }

        content()

        Row(
            modifier = Modifier
                .align(Alignment.End)
                .padding(top = 21.dp),
        ) {
            MicroSurveyFooter(
                isSubmitted = isSubmitted,
                isContentAnswerSelected = isContentAnswerSelected,
                onLinkClick = {},
                onButtonClick = {},
            )
        }
    }
}

/**
 * The preview for micro-survey open view.
 */
@Composable
@LightDarkPreview
fun MicroSurveyOpenViewPreview() {
    FirefoxTheme {
        MicroSurveyOpenView(
            isSubmitted = false,
            isContentAnswerSelected = false,
        ) {
            Card(
                shape = RoundedCornerShape(16.dp),
                backgroundColor = FirefoxTheme.colors.textPrimary,
                modifier = Modifier
                    .fillMaxWidth(),
            ) {
                Column(modifier = Modifier.height(400.dp)) {
                }
            }
        }
    }
}
