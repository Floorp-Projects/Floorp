/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.ui.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material.Colors
import androidx.compose.material.MaterialTheme
import androidx.compose.material.darkColors
import androidx.compose.material.lightColors
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.ReadOnlyComposable
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.unit.sp
import mozilla.components.ui.colors.PhotonColors

private const val TABLET_SIZE = 600

val localColors = staticCompositionLocalOf { lightColorPalette() }
val localDimensions = staticCompositionLocalOf { phoneDimensions() }

/**
 * The theme used for Firefox Focus/Klar for Android.
 */
@Composable
fun FocusTheme(
    darkTheme: Boolean = isSystemInDarkTheme(),
    content: @Composable () -> Unit,
) {
    val dimensions = if (LocalConfiguration.current.screenWidthDp <= TABLET_SIZE) {
        phoneDimensions()
    } else {
        tabletDimensions()
    }

    val colors = if (darkTheme) {
        darkColorPalette()
    } else {
        lightColorPalette()
    }

    CompositionLocalProvider(localColors provides colors, localDimensions provides dimensions) {
        MaterialTheme(
            colors = colors.material,
            typography = focusTypography.materialTypography,
            content = content,
        )
    }
}

val focusColors: FocusColors
    @Composable
    @ReadOnlyComposable
    get() = localColors.current

val focusDimensions: FocusDimensions
    @Composable
    @ReadOnlyComposable
    get() = localDimensions.current

private fun darkColorPalette(): FocusColors = FocusColors(
    material = darkColorsMaterial(),
    dialogActiveControls = PhotonColors.Pink40,
    topSiteBackground = PhotonColors.Ink05,
    topSiteFaviconText = PhotonColors.LightGrey05,
    topSiteTitle = PhotonColors.LightGrey05,
    menuBackground = PhotonColors.Ink05,
    menuText = PhotonColors.White,
    aboutPageText = PhotonColors.White,
    aboutPageLink = PhotonColors.Pink70,
    radioButtonSelected = PhotonColors.Pink70,
    toolbarColor = PhotonColors.White,
    privacySecuritySettingsToolTip = PhotonColors.White,
    onboardingKeyFeatureImageTint = PhotonColors.LightGrey05,
    onboardingButtonBackground = PhotonColors.Violet50,
    onboardingSemiBoldText = PhotonColors.LightGrey05,
    onboardingNormalText = PhotonColors.LightGrey50,
    settingsTextColor = PhotonColors.White,
    settingsTextSummaryColor = PhotonColors.LightGrey50,
    closeIcon = PhotonColors.LightGrey70,
    dialogTextColor = PhotonColors.White,
)

private fun lightColorPalette(): FocusColors = FocusColors(
    material = lightColorsMaterial(),
    dialogActiveControls = PhotonColors.Pink70,
    topSiteBackground = PhotonColors.White,
    topSiteFaviconText = PhotonColors.Ink50,
    topSiteTitle = PhotonColors.DarkGrey05,
    menuBackground = PhotonColors.White,
    menuText = PhotonColors.Black,
    aboutPageText = PhotonColors.Black,
    aboutPageLink = PhotonColors.Pink70,
    radioButtonSelected = PhotonColors.Pink70,
    toolbarColor = PhotonColors.Black,
    privacySecuritySettingsToolTip = PhotonColors.White,
    settingsTextColor = PhotonColors.Black,
    settingsTextSummaryColor = PhotonColors.DarkGrey05,
    onboardingKeyFeatureImageTint = PhotonColors.DarkGrey90,
    onboardingButtonBackground = PhotonColors.Ink20,
    onboardingSemiBoldText = PhotonColors.DarkGrey90,
    onboardingNormalText = PhotonColors.DarkGrey05,
    closeIcon = PhotonColors.LightGrey90,
    dialogTextColor = PhotonColors.Black,
)

/**
 * Material baseline colors can be overridden here.
 */
private fun darkColorsMaterial(): Colors = darkColors(
    secondary = PhotonColors.Ink05,
    surface = PhotonColors.Ink60,
    onSurface = PhotonColors.LightGrey05,
    onBackground = PhotonColors.LightGrey05,
    onPrimary = PhotonColors.LightGrey05,
)

private fun lightColorsMaterial(): Colors = lightColors(
    secondary = PhotonColors.LightGrey05,
    surface = PhotonColors.Violet05,
    onSurface = PhotonColors.Ink50,
    onBackground = PhotonColors.Ink50,
    onPrimary = PhotonColors.Ink50,
)
fun phoneDimensions() = FocusDimensions(
    onboardingTitle = 24.sp,
    onboardingSubtitleOne = 16.sp,
    onboardingSubtitleTwo = 14.sp,
)

fun tabletDimensions() = FocusDimensions(
    onboardingTitle = 28.sp,
    onboardingSubtitleOne = 18.sp,
    onboardingSubtitleTwo = 18.sp,
)
