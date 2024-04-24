/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.microsurvey.ui

import androidx.annotation.DrawableRes
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.PreviewScreenSizes
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * The UI used for micro-survey when this is opened.
 *
 * @param question The survey question text.
 * @param answers The survey answer text options available for the question.
 * @param icon The survey icon, this will represent the feature the survey is for.
 * @param isSubmitted Whether the user submitted an answer or not.
 */
@Composable
fun MicroSurveyOpenView(
    question: String,
    answers: List<String>,
    @DrawableRes icon: Int = R.drawable.ic_print, // todo currently unknown what the default will be if any.
    isSubmitted: Boolean,
) {
    val selectedAnswer = remember { mutableStateOf<String?>(null) }

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

        MicroSurveyContent(
            question = question,
            icon = icon,
            answers = answers,
            selectedAnswer = selectedAnswer.value,
            onSelectionChange = { selectedAnswer.value = it },
        )

        Row(
            modifier = Modifier
                .align(Alignment.End)
                .padding(top = 21.dp),
        ) {
            MicroSurveyFooter(
                isSubmitted = isSubmitted,
                isContentAnswerSelected = selectedAnswer.value != null,
                onLinkClick = {},
                onButtonClick = {},
            )
        }
    }
}

/**
 * The preview for micro-survey open view.
 */
@PreviewScreenSizes
@LightDarkPreview
@Composable
fun MicroSurveyOpenViewPreview() {
    FirefoxTheme {
        MicroSurveyOpenView(
            question = "How satisfied are you with printing in Firefox?",
            icon = R.drawable.ic_print,
            answers = listOf(
                stringResource(id = R.string.likert_scale_option_1),
                stringResource(id = R.string.likert_scale_option_2),
                stringResource(id = R.string.likert_scale_option_3),
                stringResource(id = R.string.likert_scale_option_4),
                stringResource(id = R.string.likert_scale_option_5),
            ),
            isSubmitted = false,
        )
    }
}
