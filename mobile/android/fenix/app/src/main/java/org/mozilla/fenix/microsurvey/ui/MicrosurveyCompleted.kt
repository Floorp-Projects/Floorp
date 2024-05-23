/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.microsurvey.ui

import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.PreviewScreenSizes
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * The microsurvey view to show the survey was submitted.
 *
 * @param backgroundColor The view background color.
 */
@Composable
fun MicrosurveyCompleted(
    backgroundColor: Color = FirefoxTheme.colors.layer2,
) {
    val shape = RoundedCornerShape(8.dp)
    val elevation: Dp = 5.dp

    Card(
        shape = shape,
        backgroundColor = backgroundColor,
        elevation = elevation,
        modifier = Modifier
            .wrapContentHeight()
            .fillMaxWidth(),
    ) {
        Column(
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.SpaceEvenly,
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
        ) {
            Spacer(modifier = Modifier.height(50.dp))

            Row {
                Image(
                    painter = painterResource(R.drawable.microsurvey_success),
                    contentDescription = null,
                    contentScale = ContentScale.None,
                )
            }

            Spacer(modifier = Modifier.height(40.dp))

            Row {
                Text(
                    text = stringResource(id = R.string.micro_survey_feedback_confirmation),
                    style = FirefoxTheme.typography.headline7,
                    color = FirefoxTheme.colors.textPrimary,
                )
            }

            Spacer(modifier = Modifier.height(60.dp))
        }
    }
}

/**
 * Preview for [MicrosurveyCompleted].
 */
@PreviewScreenSizes
@LightDarkPreview
@Composable
fun MicroSurveyCompletedPreview() {
    FirefoxTheme {
        MicrosurveyCompleted()
    }
}
