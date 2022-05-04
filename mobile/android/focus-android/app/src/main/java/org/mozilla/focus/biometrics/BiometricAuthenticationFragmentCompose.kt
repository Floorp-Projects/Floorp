/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.biometrics

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Button
import androidx.compose.material.ButtonDefaults
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.colorResource
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import mozilla.components.ui.colors.PhotonColors
import org.mozilla.focus.R
import org.mozilla.focus.ui.theme.FocusTheme
import org.mozilla.focus.ui.theme.focusTypography

@Composable
@Preview
private fun BiometricPromptContentPreview() {
    FocusTheme {
        BiometricPromptContent("Fingerprint operation canceled by user.") {}
    }
}

@Composable
fun BiometricPromptContent(biometricErrorText: String, showBiometricPrompt: () -> Unit) {
    Column(
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center,
        modifier = Modifier
            .fillMaxWidth()
            .fillMaxHeight()
            .background(
                brush = Brush.linearGradient(
                    colors = listOf(
                        colorResource(R.color.biometric_background_fragment_one),
                        colorResource(R.color.biometric_background_fragment_two),
                        colorResource(R.color.biometric_background_fragment_three),
                        colorResource(R.color.biometric_background_fragment_four),
                        colorResource(R.color.biometric_background_fragment_five),
                        colorResource(R.color.biometric_background_fragment_six),
                    ),
                    end = Offset(0f, Float.POSITIVE_INFINITY),
                    start = Offset(Float.POSITIVE_INFINITY, 0f)
                )
            )
    ) {
        Image(
            painter = painterResource(R.drawable.wordmark2),
            contentDescription = LocalContext.current.getString(R.string.app_name),
            modifier = Modifier
                .padding(start = 24.dp, end = 24.dp)
        )
        Text(
            style = focusTypography.onboardingButton,
            color = Color.Red,
            text = biometricErrorText,
            modifier = Modifier.padding(top = 16.dp, bottom = 16.dp)
        )
        ComponentShowBiometricPromptButton {
            showBiometricPrompt()
        }
    }
}

@Composable
private fun ComponentShowBiometricPromptButton(showBiometricPrompt: () -> Unit) {
    Button(
        onClick = showBiometricPrompt,
        colors = ButtonDefaults.textButtonColors(
            backgroundColor = colorResource(R.color.biometric_show_button_background)
        ),
        modifier = Modifier
            .padding(16.dp)
            .fillMaxWidth()
    ) {
        Image(
            painter = painterResource(R.drawable.ic_fingerprint),
            contentDescription = LocalContext.current.getString(R.string.biometric_auth_image_description),
            modifier = Modifier
                .padding(end = 10.dp)
        )
        Text(
            color = PhotonColors.White,
            text = AnnotatedString(
                LocalContext.current.resources.getString(
                    R.string.show_biometric_button_text
                )
            )
        )
    }
}
