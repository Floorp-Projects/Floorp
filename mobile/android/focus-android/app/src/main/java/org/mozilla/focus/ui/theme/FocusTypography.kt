/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.ui.theme

import androidx.compose.material.Typography
import androidx.compose.runtime.Composable
import androidx.compose.runtime.ReadOnlyComposable
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.unit.sp
import mozilla.components.ui.colors.PhotonColors
import org.mozilla.focus.R

val metropolis = FontFamily(
    Font(R.font.metropolis, FontWeight.Normal),
    Font(R.font.metropolis_bold, FontWeight.Bold),
    Font(R.font.metropolis_semibold, FontWeight.SemiBold),
)

/**
 * Custom Focus typography, other than baseline Material typography.
 * Custom text styles should be added with consideration,
 * only when an existing Material style is not suitable.
 */
data class FocusTypography(
    val materialTypography: Typography,
    val links: TextStyle,
    val dialogTitle: TextStyle,
    val dialogInput: TextStyle,
    val dialogContent: TextStyle,
    val onboardingTitle: TextStyle,
    val onboardingSubtitle: TextStyle,
    val onboardingDescription: TextStyle,
    val onboardingFeatureTitle: TextStyle,
    val onboardingFeatureDescription: TextStyle,
    val onboardingButton: TextStyle,
    val cfrTextStyle: TextStyle,
    val cfrCookieBannerTextStyle: TextStyle,
) {
    val h1: TextStyle get() = materialTypography.h1
    val h2: TextStyle get() = materialTypography.h2
    val h3: TextStyle get() = materialTypography.h3
    val h4: TextStyle get() = materialTypography.h4
    val h5: TextStyle get() = materialTypography.h5
    val h6: TextStyle get() = materialTypography.h6
    val subtitle1: TextStyle get() = materialTypography.subtitle1
    val subtitle2: TextStyle get() = materialTypography.subtitle2
    val body1: TextStyle get() = materialTypography.body1
    val body2: TextStyle get() = materialTypography.body2
    val button: TextStyle get() = materialTypography.button
    val caption: TextStyle get() = materialTypography.caption
    val overline: TextStyle get() = materialTypography.overline
}

val focusTypography: FocusTypography
    @Composable
    @ReadOnlyComposable
    get() = FocusTypography(
        materialTypography = Typography(
            body1 = TextStyle(
                fontSize = 16.sp,
                lineHeight = 24.sp,
            ),
        ),
        links = TextStyle(
            fontSize = 16.sp,
            textDecoration = TextDecoration.Underline,
            lineHeight = 24.sp,
        ),
        dialogTitle = TextStyle(
            fontFamily = metropolis,
            fontWeight = FontWeight.SemiBold,
            fontSize = 20.sp,
        ),
        dialogInput = TextStyle(
            fontFamily = metropolis,
            fontSize = 20.sp,
            color = focusColors.onPrimary,
        ),
        dialogContent = TextStyle(
            fontFamily = metropolis,
            fontWeight = FontWeight.Normal,
            fontSize = 14.sp,
        ),
        onboardingTitle = TextStyle(
            fontFamily = metropolis,
            fontWeight = FontWeight.SemiBold,
            fontSize = focusDimensions.onboardingTitle,
            color = focusColors.onboardingSemiBoldText,
        ),
        onboardingSubtitle = TextStyle(
            fontFamily = metropolis,
            fontWeight = FontWeight.Normal,
            fontSize = focusDimensions.onboardingSubtitleOne,
            color = focusColors.onboardingSemiBoldText,
        ),
        onboardingDescription = TextStyle(
            fontFamily = metropolis,
            fontWeight = FontWeight.Normal,
            fontSize = focusDimensions.onboardingSubtitleTwo,
            color = focusColors.onboardingNormalText,
        ),
        onboardingFeatureTitle = TextStyle(
            fontFamily = metropolis,
            fontWeight = FontWeight.SemiBold,
            fontSize = 14.sp,
            color = focusColors.onboardingSemiBoldText,
        ),
        onboardingFeatureDescription = TextStyle(
            fontFamily = metropolis,
            fontWeight = FontWeight.Normal,
            fontSize = 14.sp,
            color = focusColors.onboardingNormalText,
        ),
        onboardingButton = TextStyle(
            fontFamily = metropolis,
            fontWeight = FontWeight.SemiBold,
            fontSize = 14.sp,
            color = PhotonColors.LightGrey05,
        ),
        cfrTextStyle = TextStyle(
            fontSize = 16.sp,
            letterSpacing = 0.5.sp,
            lineHeight = 24.sp,
        ),
        cfrCookieBannerTextStyle = TextStyle(
            fontSize = 14.sp,
            letterSpacing = 0.25.sp,
            lineHeight = 20.sp,
        ),
    )
