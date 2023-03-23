/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.experiments.view

import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.compose.button.SecondaryButton
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * The ratio of the content height to screen height. This was determined from the designs in figma
 * taking the top and bottom padding to be 10% of screen height.
 */
private const val FULLSCREEN_HEIGHT = 0.8f

/**
 * The ratio of the button width to screen width. This was determined from the designs in figma
 * taking the horizontal button paddings to be 5% of the screen width.
 */
private const val BUTTON_WIDTH = 0.9f

/**
 * A full screen for displaying a research surface.
 *
 * @param messageText The research surface message text to be displayed.
 * @param onAcceptButtonText A positive button text of the fullscreen message.
 * @param onDismissButtonText A negative button text of the fullscreen message.
 * @param onDismiss Invoked when the user clicks on the "No Thanks" button.
 * @param onAccept Invoked when the user clicks on the "Take Survey" button
 */
@Composable
fun ResearchSurfaceSurvey(
    messageText: String,
    onAcceptButtonText: String,
    onDismissButtonText: String,
    onDismiss: () -> Unit,
    onAccept: () -> Unit,
) {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .statusBarsPadding()
            .navigationBarsPadding(),
    ) {
        Column(
            modifier = Modifier
                .fillMaxHeight(FULLSCREEN_HEIGHT)
                .align(Alignment.Center)
                .verticalScroll(rememberScrollState()),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.SpaceBetween,
        ) {
            Spacer(Modifier)
            Column(
                modifier = Modifier.padding(horizontal = 16.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                Image(
                    painter = painterResource(R.drawable.ic_firefox),
                    contentDescription = null,
                )
                Spacer(modifier = Modifier.height(16.dp))
                Text(
                    text = messageText,
                    color = FirefoxTheme.colors.textPrimary,
                    textAlign = TextAlign.Center,
                    style = FirefoxTheme.typography.headline6,
                )
            }
            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                modifier = Modifier.fillMaxWidth(BUTTON_WIDTH),
            ) {
                PrimaryButton(
                    text = onAcceptButtonText,
                    onClick = onAccept,
                )
                Spacer(modifier = Modifier.height(8.dp))
                SecondaryButton(
                    text = onDismissButtonText,
                    onClick = onDismiss,
                )
            }
        }
    }
}

@Composable
@LightDarkPreview
private fun SurveyPreview() {
    FirefoxTheme {
        ResearchSurfaceSurvey(
            messageText = stringResource(id = R.string.nimbus_survey_message_text),
            onAcceptButtonText = stringResource(id = R.string.preferences_take_survey),
            onDismissButtonText = stringResource(id = R.string.preferences_not_take_survey),
            onDismiss = {},
            onAccept = {},
        )
    }
}
