/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.ui.theme

import androidx.compose.material.Colors
import androidx.compose.ui.graphics.Color

/**
 * Custom Focus colors, other than baseline Material color theme.
 * Colors here should be added with consideration, only when an existing Material baseline color is not suitable.
 */
data class FocusColors(
    val material: Colors,
    val dialogActiveControls: Color,
    val topSiteBackground: Color,
    val topSiteFaviconText: Color,
    val topSiteTitle: Color,
    val menuBackground: Color,
    val menuText: Color,
    val aboutPageText: Color,
    val aboutPageLink: Color,
    val radioButtonSelected: Color,
    val toolbarColor: Color,
    val privacySecuritySettingsToolTip: Color,
    val onboardingButtonBackground: Color,
    val onboardingKeyFeatureImageTint: Color,
    val onboardingSemiBoldText: Color,
    val onboardingNormalText: Color,
    val settingsTextColor: Color,
    val settingsTextSummaryColor: Color,
    val closeIcon: Color,
    val dialogTextColor: Color,
) {
    val primary: Color get() = material.primary
    val primaryVariant: Color get() = material.primaryVariant
    val secondary: Color get() = material.secondary
    val secondaryVariant: Color get() = material.secondaryVariant
    val background: Color get() = material.background
    val surface: Color get() = material.surface
    val error: Color get() = material.error
    val onPrimary: Color get() = material.onPrimary
    val onSecondary: Color get() = material.onSecondary
    val onBackground: Color get() = material.onBackground
    val onSurface: Color get() = material.onSurface
    val onError: Color get() = material.onError
    val isLight: Boolean get() = material.isLight
}
