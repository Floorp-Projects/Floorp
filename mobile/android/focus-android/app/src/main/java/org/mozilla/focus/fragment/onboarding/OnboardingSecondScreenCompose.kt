/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment.onboarding

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.Button
import androidx.compose.material.ButtonDefaults
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.colorResource
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import mozilla.components.ui.colors.PhotonColors
import org.mozilla.focus.R
import org.mozilla.focus.ui.theme.FocusTheme
import org.mozilla.focus.ui.theme.focusColors
import org.mozilla.focus.ui.theme.focusTypography

@Composable
@Preview
private fun OnBoardingSecondScreenComposePreview() {
    FocusTheme {
        OnBoardingSecondScreenCompose({}, {}, {})
    }
}

/**
 * Displays the second onBoarding screen
 *
 * @param setAsDefaultBrowser Will be called when the user clicks on SetDefaultBrowser button.
 * @param skipScreen Will be called when the user clicks on Skip button.
 * @param onCloseButtonClick The lambda to be invoked when close button icon is pressed.
 */
@Composable
@Suppress("LongMethod")
fun OnBoardingSecondScreenCompose(
    setAsDefaultBrowser: () -> Unit,
    skipScreen: () -> Unit,
    onCloseButtonClick: () -> Unit,
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(
                brush = Brush.linearGradient(
                    colors = listOf(
                        colorResource(R.color.home_screen_modal_gradient_one),
                        colorResource(R.color.home_screen_modal_gradient_two),
                        colorResource(R.color.home_screen_modal_gradient_three),
                        colorResource(R.color.home_screen_modal_gradient_four),
                        colorResource(R.color.home_screen_modal_gradient_five),
                        colorResource(R.color.home_screen_modal_gradient_six),
                    ),
                    end = Offset(0f, Float.POSITIVE_INFINITY),
                    start = Offset(Float.POSITIVE_INFINITY, 0f),
                ),
            ),
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(top = 60.dp, end = 20.dp),
            horizontalAlignment = Alignment.End,
        ) {
            CloseButton(onCloseButtonClick)
        }
        Column(
            modifier = Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.Center,
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Image(
                painter = painterResource(R.drawable.onboarding_second_screen_icon),
                contentDescription = LocalContext.current.getString(R.string.app_name),
                modifier = Modifier
                    .size(200.dp, 300.dp),
            )
            Text(
                text = stringResource(
                    R.string.onboarding_second_screen_title,
                    stringResource(R.string.onboarding_short_app_name),
                ),
                modifier = Modifier
                    .padding(top = 32.dp, start = 16.dp, end = 16.dp),
                textAlign = TextAlign.Center,
                style = focusTypography.onboardingTitle,
            )
            Text(
                text = stringResource(
                    R.string.onboarding_second_screen_subtitle_one,
                ),
                modifier = Modifier
                    .padding(top = 16.dp, start = 16.dp, end = 16.dp),
                textAlign = TextAlign.Center,
                style = focusTypography.onboardingSubtitle,
            )
            Text(
                text = stringResource(
                    R.string.onboarding_second_screen_subtitle_two,
                    stringResource(R.string.onboarding_short_app_name),
                ),
                modifier = Modifier
                    .padding(top = 16.dp, start = 16.dp, end = 16.dp),
                textAlign = TextAlign.Center,
                style = focusTypography.onboardingSubtitle,
            )
            ComponentOnBoardingSecondScreenButtons(setAsDefaultBrowser, skipScreen)
        }
    }
}

@Composable
private fun CloseButton(onCloseButtonClick: () -> Unit) {
    IconButton(
        modifier = Modifier
            .size(48.dp)
            .background(
                colorResource(id = R.color.onboardingCloseButtonColor),
                shape = CircleShape,
            ),
        onClick = onCloseButtonClick,
    ) {
        Icon(
            painter = painterResource(R.drawable.mozac_ic_cross_24),
            contentDescription = stringResource(R.string.onboarding_close_button_content_description),
            tint = focusColors.closeIcon,
        )
    }
}

@Composable
private fun ComponentOnBoardingSecondScreenButtons(setAsDefaultBrowser: () -> Unit, skipScreen: () -> Unit) {
    Button(
        onClick = setAsDefaultBrowser,
        modifier = Modifier
            .padding(top = 33.dp, start = 16.dp, end = 16.dp)
            .fillMaxWidth(),
        colors = ButtonDefaults.textButtonColors(
            backgroundColor = colorResource(R.color.onboardingButtonOneColor),
        ),
    ) {
        Text(
            text = AnnotatedString(
                LocalContext.current.resources.getString(
                    R.string.onboarding_second_screen_default_browser_button_text,
                ),
            ),
            color = PhotonColors.White,
        )
    }
    Button(
        onClick = skipScreen,
        modifier = Modifier
            .padding(top = 8.dp, start = 16.dp, end = 16.dp)
            .fillMaxWidth(),
        colors = ButtonDefaults.textButtonColors(
            backgroundColor = colorResource(R.color.onboardingButtonTwoColor),
        ),
    ) {
        Text(
            text = AnnotatedString(
                LocalContext.current.resources.getString(
                    R.string.onboarding_second_screen_skip_button_text,
                ),
            ),
            color = PhotonColors.Black,
        )
    }
}
