/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.microsurvey.ui

import androidx.annotation.DrawableRes
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.Surface
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.traversalIndex
import androidx.compose.ui.tooling.preview.PreviewScreenSizes
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.BottomSheetHandle
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

private const val BOTTOM_SHEET_HANDLE_WIDTH_PERCENT = 0.1f
private val bottomSheetShape = RoundedCornerShape(topStart = 16.dp, topEnd = 16.dp)

/**
 * The microsurvey bottom sheet.
 *
 * @param question The question text.
 * @param answers The answer text options available for the given [question].
 * @param icon The icon that represents the feature for the given [question].
 */
@Composable
fun MicrosurveyBottomSheet(
    question: String,
    answers: List<String>,
    @DrawableRes icon: Int = R.drawable.ic_print, // todo currently unknown if default is used FXDROID-1921.
) {
    var selectedAnswer by remember { mutableStateOf<String?>(null) }
    var isSubmitted by remember { mutableStateOf(false) }

    Surface(
        color = FirefoxTheme.colors.layer1,
        shape = bottomSheetShape,
    ) {
        Column(
            modifier = Modifier
                .padding(
                    vertical = 8.dp,
                    horizontal = 16.dp,
                ),
        ) {
            BottomSheetHandle(
                onRequestDismiss = {},
                contentDescription = stringResource(R.string.review_quality_check_close_handle_content_description),
                modifier = Modifier
                    .fillMaxWidth(BOTTOM_SHEET_HANDLE_WIDTH_PERCENT)
                    .align(Alignment.CenterHorizontally)
                    .semantics { traversalIndex = -1f },
            )

            Spacer(modifier = Modifier.height(4.dp))

            MicroSurveyHeader(title = stringResource(id = R.string.micro_survey_survey_header_2)) {}

            Spacer(modifier = Modifier.height(8.dp))

            Column(modifier = Modifier.verticalScroll(rememberScrollState())) {
                if (isSubmitted) {
                    MicrosurveyCompleted()
                } else {
                    MicroSurveyContent(
                        question = question,
                        icon = icon,
                        answers = answers,
                        selectedAnswer = selectedAnswer,
                        onSelectionChange = { selectedAnswer = it },
                    )
                }

                Spacer(modifier = Modifier.height(24.dp))

                MicroSurveyFooter(
                    isSubmitted = isSubmitted,
                    isContentAnswerSelected = selectedAnswer != null,
                    onLinkClick = {}, // todo add privacy policy link and open new tab FXDROID-1876.
                    onButtonClick = { isSubmitted = true },
                )
            }
        }
    }
}

@PreviewScreenSizes
@LightDarkPreview
@Composable
private fun MicroSurveyBottomSheetPreview() {
    FirefoxTheme {
        MicrosurveyBottomSheet(
            question = "How satisfied are you with printing in Firefox?",
            icon = R.drawable.ic_print,
            answers = listOf(
                stringResource(id = R.string.likert_scale_option_1),
                stringResource(id = R.string.likert_scale_option_2),
                stringResource(id = R.string.likert_scale_option_3),
                stringResource(id = R.string.likert_scale_option_4),
                stringResource(id = R.string.likert_scale_option_5),
            ),
        )
    }
}
