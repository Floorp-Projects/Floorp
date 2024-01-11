/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("MagicNumber")

package org.mozilla.fenix.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.Stable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import mozilla.components.ui.colors.PhotonColors
import org.mozilla.fenix.compose.inComposePreview
import org.mozilla.fenix.ext.settings

/**
 * Indicates the theme that is displayed.
 */
enum class Theme {
    Light,
    Dark,
    Private,
    ;

    companion object {
        /**
         * Returns the current [Theme] that is displayed.
         *
         * @param allowPrivateTheme Boolean used to control whether [Theme.Private] is an option
         * for [FirefoxTheme] colors.
         *
         * @return the current [Theme] that is displayed.
         */
        @Composable
        fun getTheme(allowPrivateTheme: Boolean = true) =
            if (allowPrivateTheme &&
                !inComposePreview &&
                LocalContext.current.settings().lastKnownMode.isPrivate
            ) {
                Private
            } else if (isSystemInDarkTheme()) {
                Dark
            } else {
                Light
            }
    }
}

/**
 * The theme for Mozilla Firefox for Android (Fenix).
 *
 * @param theme The current [Theme] that is displayed.
 * @param content The children composables to be laid out.
 */
@Composable
fun FirefoxTheme(
    theme: Theme = Theme.getTheme(),
    content: @Composable () -> Unit,
) {
    val colors = when (theme) {
        Theme.Light -> lightColorPalette
        Theme.Dark -> darkColorPalette
        Theme.Private -> privateColorPalette
    }

    ProvideFirefoxColors(colors) {
        MaterialTheme(
            content = content,
        )
    }
}

object FirefoxTheme {
    val colors: FirefoxColors
        @Composable
        get() = localFirefoxColors.current

    val typography: FenixTypography
        get() = defaultTypography
}

private val darkColorPalette = FirefoxColors(
    layer1 = PhotonColors.DarkGrey60,
    layer2 = PhotonColors.DarkGrey30,
    layer3 = PhotonColors.DarkGrey80,
    layer4Start = PhotonColors.Purple70,
    layer4Center = PhotonColors.Violet80,
    layer4End = PhotonColors.Ink05,
    layerAccent = PhotonColors.Violet40,
    layerAccentNonOpaque = PhotonColors.Violet50A32,
    layerAccentOpaque = Color(0xFF423262),
    scrim = PhotonColors.DarkGrey90A95,
    gradientStart = PhotonColors.Violet70,
    gradientEnd = PhotonColors.Violet40,
    layerWarning = PhotonColors.Yellow70A77,
    layerConfirmation = PhotonColors.Green80,
    layerError = PhotonColors.Pink80,
    layerInfo = PhotonColors.Blue50,
    actionPrimary = PhotonColors.Violet60,
    actionSecondary = PhotonColors.LightGrey30,
    actionTertiary = PhotonColors.DarkGrey10,
    actionQuarternary = PhotonColors.DarkGrey80,
    actionWarning = PhotonColors.Yellow40A41,
    actionConfirmation = PhotonColors.Green70,
    actionError = PhotonColors.Pink70A69,
    actionInfo = PhotonColors.Blue60,
    formDefault = PhotonColors.LightGrey05,
    formSelected = PhotonColors.Violet40,
    formSurface = PhotonColors.DarkGrey05,
    formDisabled = PhotonColors.DarkGrey05,
    formOn = PhotonColors.Violet40,
    formOff = PhotonColors.LightGrey05,
    indicatorActive = PhotonColors.LightGrey90,
    indicatorInactive = PhotonColors.DarkGrey05,
    textPrimary = PhotonColors.LightGrey05,
    textSecondary = PhotonColors.LightGrey40,
    textDisabled = PhotonColors.LightGrey05A40,
    textWarning = PhotonColors.Red20,
    textWarningButton = PhotonColors.Red70,
    textAccent = PhotonColors.Violet20,
    textAccentDisabled = PhotonColors.Violet20A60,
    textOnColorPrimary = PhotonColors.LightGrey05,
    textOnColorSecondary = PhotonColors.LightGrey40,
    textActionPrimary = PhotonColors.LightGrey05,
    textActionSecondary = PhotonColors.DarkGrey90,
    textActionTertiary = PhotonColors.LightGrey05,
    textActionTertiaryActive = PhotonColors.LightGrey05,
    iconPrimary = PhotonColors.LightGrey05,
    iconPrimaryInactive = PhotonColors.LightGrey05A60,
    iconSecondary = PhotonColors.LightGrey40,
    iconActive = PhotonColors.Violet40,
    iconDisabled = PhotonColors.LightGrey05A40,
    iconOnColor = PhotonColors.LightGrey05,
    iconNotice = PhotonColors.Blue30,
    iconButton = PhotonColors.LightGrey05,
    iconWarning = PhotonColors.Red20,
    iconWarningButton = PhotonColors.Red70,
    iconAccentViolet = PhotonColors.Violet20,
    iconAccentBlue = PhotonColors.Blue20,
    iconAccentPink = PhotonColors.Pink20,
    iconAccentGreen = PhotonColors.Green20,
    iconAccentYellow = PhotonColors.Yellow20,
    iconActionPrimary = PhotonColors.LightGrey05,
    iconActionSecondary = PhotonColors.DarkGrey90,
    iconActionTertiary = PhotonColors.LightGrey05,
    iconGradientStart = PhotonColors.Violet20,
    iconGradientEnd = PhotonColors.Blue20,
    borderPrimary = PhotonColors.DarkGrey05,
    borderInverted = PhotonColors.LightGrey30,
    borderFormDefault = PhotonColors.LightGrey05,
    borderAccent = PhotonColors.Violet40,
    borderDisabled = PhotonColors.LightGrey05A40,
    borderWarning = PhotonColors.Red40,
)

private val lightColorPalette = FirefoxColors(
    layer1 = PhotonColors.LightGrey10,
    layer2 = PhotonColors.White,
    layer3 = PhotonColors.LightGrey20,
    layer4Start = PhotonColors.Purple70,
    layer4Center = PhotonColors.Violet80,
    layer4End = PhotonColors.Ink05,
    layerAccent = PhotonColors.Ink20,
    layerAccentNonOpaque = PhotonColors.Violet70A12,
    layerAccentOpaque = Color(0xFFEAE4F9),
    scrim = PhotonColors.DarkGrey30A95,
    gradientStart = PhotonColors.Violet70,
    gradientEnd = PhotonColors.Violet40,
    layerWarning = PhotonColors.Yellow20,
    layerConfirmation = PhotonColors.Green20,
    layerError = PhotonColors.Red10,
    layerInfo = PhotonColors.Blue50A44,
    actionPrimary = PhotonColors.Ink20,
    actionSecondary = PhotonColors.LightGrey30,
    actionTertiary = PhotonColors.LightGrey40,
    actionQuarternary = PhotonColors.LightGrey10,
    actionWarning = PhotonColors.Yellow60A40,
    actionConfirmation = PhotonColors.Green60,
    actionError = PhotonColors.Red30,
    actionInfo = PhotonColors.Blue50,
    formDefault = PhotonColors.DarkGrey90,
    formSelected = PhotonColors.Ink20,
    formSurface = PhotonColors.LightGrey50,
    formDisabled = PhotonColors.LightGrey50,
    formOn = PhotonColors.Ink20,
    formOff = PhotonColors.LightGrey05,
    indicatorActive = PhotonColors.LightGrey50,
    indicatorInactive = PhotonColors.LightGrey30,
    textPrimary = PhotonColors.DarkGrey90,
    textSecondary = PhotonColors.DarkGrey05,
    textDisabled = PhotonColors.DarkGrey90A40,
    textWarning = PhotonColors.Red70,
    textWarningButton = PhotonColors.Red70,
    textAccent = PhotonColors.Violet70,
    textAccentDisabled = PhotonColors.Violet70A80,
    textOnColorPrimary = PhotonColors.LightGrey05,
    textOnColorSecondary = PhotonColors.LightGrey40,
    textActionPrimary = PhotonColors.LightGrey05,
    textActionSecondary = PhotonColors.DarkGrey90,
    textActionTertiary = PhotonColors.DarkGrey90,
    textActionTertiaryActive = PhotonColors.LightGrey05,
    iconPrimary = PhotonColors.DarkGrey90,
    iconPrimaryInactive = PhotonColors.DarkGrey90A60,
    iconSecondary = PhotonColors.DarkGrey05,
    iconActive = PhotonColors.Ink20,
    iconDisabled = PhotonColors.DarkGrey90A40,
    iconOnColor = PhotonColors.LightGrey05,
    iconNotice = PhotonColors.Blue30,
    iconButton = PhotonColors.Ink20,
    iconWarning = PhotonColors.Red70,
    iconWarningButton = PhotonColors.Red70,
    iconAccentViolet = PhotonColors.Violet60,
    iconAccentBlue = PhotonColors.Blue60,
    iconAccentPink = PhotonColors.Pink60,
    iconAccentGreen = PhotonColors.Green60,
    iconAccentYellow = PhotonColors.Yellow60,
    iconActionPrimary = PhotonColors.LightGrey05,
    iconActionSecondary = PhotonColors.DarkGrey90,
    iconActionTertiary = PhotonColors.DarkGrey90,
    iconGradientStart = PhotonColors.Violet50,
    iconGradientEnd = PhotonColors.Blue60,
    borderPrimary = PhotonColors.LightGrey30,
    borderInverted = PhotonColors.DarkGrey05,
    borderFormDefault = PhotonColors.DarkGrey90,
    borderAccent = PhotonColors.Ink20,
    borderDisabled = PhotonColors.DarkGrey90A40,
    borderWarning = PhotonColors.Red70,
)

private val privateColorPalette = darkColorPalette.copy(
    layer1 = PhotonColors.Ink50,
    layer2 = PhotonColors.Ink50,
    layer3 = PhotonColors.Ink90,
)

/**
 * A custom Color Palette for Mozilla Firefox for Android (Fenix).
 */
@Suppress("LargeClass", "LongParameterList")
@Stable
class FirefoxColors(
    layer1: Color,
    layer2: Color,
    layer3: Color,
    layer4Start: Color,
    layer4Center: Color,
    layer4End: Color,
    layerAccent: Color,
    layerAccentNonOpaque: Color,
    layerAccentOpaque: Color,
    scrim: Color,
    gradientStart: Color,
    gradientEnd: Color,
    layerWarning: Color,
    layerConfirmation: Color,
    layerError: Color,
    layerInfo: Color,
    actionPrimary: Color,
    actionSecondary: Color,
    actionTertiary: Color,
    actionQuarternary: Color,
    actionWarning: Color,
    actionConfirmation: Color,
    actionError: Color,
    actionInfo: Color,
    formDefault: Color,
    formSelected: Color,
    formSurface: Color,
    formDisabled: Color,
    formOn: Color,
    formOff: Color,
    indicatorActive: Color,
    indicatorInactive: Color,
    textPrimary: Color,
    textSecondary: Color,
    textDisabled: Color,
    textWarning: Color,
    textWarningButton: Color,
    textAccent: Color,
    textAccentDisabled: Color,
    textOnColorPrimary: Color,
    textOnColorSecondary: Color,
    textActionPrimary: Color,
    textActionSecondary: Color,
    textActionTertiary: Color,
    textActionTertiaryActive: Color,
    iconPrimary: Color,
    iconPrimaryInactive: Color,
    iconSecondary: Color,
    iconActive: Color,
    iconDisabled: Color,
    iconOnColor: Color,
    iconNotice: Color,
    iconButton: Color,
    iconWarning: Color,
    iconWarningButton: Color,
    iconAccentViolet: Color,
    iconAccentBlue: Color,
    iconAccentPink: Color,
    iconAccentGreen: Color,
    iconAccentYellow: Color,
    iconActionPrimary: Color,
    iconActionSecondary: Color,
    iconActionTertiary: Color,
    iconGradientStart: Color,
    iconGradientEnd: Color,
    borderPrimary: Color,
    borderInverted: Color,
    borderFormDefault: Color,
    borderAccent: Color,
    borderDisabled: Color,
    borderWarning: Color,
) {
    // Layers

    // Default Screen background, Frontlayer background, App Bar Top, App Bar Bottom, Frontlayer header
    var layer1 by mutableStateOf(layer1)
        private set

    // Card background, Menu background, Dialog, Banner
    var layer2 by mutableStateOf(layer2)
        private set

    // Search
    var layer3 by mutableStateOf(layer3)
        private set

    // Homescreen background, Toolbar
    var layer4Start by mutableStateOf(layer4Start)
        private set

    // Homescreen background, Toolbar
    var layer4Center by mutableStateOf(layer4Center)
        private set

    // Homescreen background, Toolbar
    var layer4End by mutableStateOf(layer4End)
        private set

    // App Bar Top (edit), Text Cursor, Selected Tab Check
    var layerAccent by mutableStateOf(layerAccent)
        private set

    // Selected tab
    var layerAccentNonOpaque by mutableStateOf(layerAccentNonOpaque)
        private set

    // Selected tab
    var layerAccentOpaque by mutableStateOf(layerAccentOpaque)
        private set

    var scrim by mutableStateOf(scrim)
        private set

    // Tooltip
    var gradientStart by mutableStateOf(gradientStart)
        private set

    // Tooltip
    var gradientEnd by mutableStateOf(gradientEnd)
        private set

    // Warning background
    var layerWarning by mutableStateOf(layerWarning)
        private set

    // Confirmation background
    var layerConfirmation by mutableStateOf(layerConfirmation)
        private set

    // Error Background
    var layerError by mutableStateOf(layerError)
        private set

    // Info background
    var layerInfo by mutableStateOf(layerInfo)
        private set

    // Actions

    // Primary button, Snackbar, Floating action button, Chip selected
    var actionPrimary by mutableStateOf(actionPrimary)
        private set

    // Secondary button
    var actionSecondary by mutableStateOf(actionSecondary)
        private set

    // Filter
    var actionTertiary by mutableStateOf(actionTertiary)
        private set

    // Chip
    var actionQuarternary by mutableStateOf(actionQuarternary)
        private set

    // Warning button
    var actionWarning by mutableStateOf(actionWarning)
        private set

    // Confirmation button
    var actionConfirmation by mutableStateOf(actionConfirmation)
        private set

    // Error button
    var actionError by mutableStateOf(actionError)
        private set

    // Info button
    var actionInfo by mutableStateOf(actionInfo)
        private set

    // Checkbox default, Radio button default
    var formDefault by mutableStateOf(formDefault)
        private set

    // Checkbox selected, Radio button selected
    var formSelected by mutableStateOf(formSelected)
        private set

    // Switch background OFF, Switch background ON
    var formSurface by mutableStateOf(formSurface)
        private set

    // Checkbox disabled, Radio disabled
    var formDisabled by mutableStateOf(formDisabled)
        private set

    // Switch thumb ON
    var formOn by mutableStateOf(formOn)
        private set

    // Switch thumb OFF
    var formOff by mutableStateOf(formOff)
        private set

    // Scroll indicator active
    var indicatorActive by mutableStateOf(indicatorActive)
        private set

    // Scroll indicator inactive
    var indicatorInactive by mutableStateOf(indicatorInactive)
        private set

    // Text

    // Primary text
    var textPrimary by mutableStateOf(textPrimary)
        private set

    // Secondary text
    var textSecondary by mutableStateOf(textSecondary)
        private set

    // Disabled text
    var textDisabled by mutableStateOf(textDisabled)
        private set

    // Warning text
    var textWarning by mutableStateOf(textWarning)
        private set

    // Warning text on Secondary button
    var textWarningButton by mutableStateOf(textWarningButton)
        private set

    // Small heading, Text link
    var textAccent by mutableStateOf(textAccent)
        private set

    // Small heading, Text link
    var textAccentDisabled by mutableStateOf(textAccentDisabled)
        private set

    // Text Inverted/On Color
    var textOnColorPrimary by mutableStateOf(textOnColorPrimary)
        private set

    // Text Inverted/On Color
    var textOnColorSecondary by mutableStateOf(textOnColorSecondary)
        private set

    // Action Primary text
    var textActionPrimary by mutableStateOf(textActionPrimary)
        private set

    // Action Secondary text
    var textActionSecondary by mutableStateOf(textActionSecondary)
        private set

    // Action Tertiary text
    var textActionTertiary by mutableStateOf(textActionTertiary)
        private set

    // Action Tertiary Active text
    var textActionTertiaryActive by mutableStateOf(textActionTertiaryActive)
        private set

    // Icon

    // Primary icon
    var iconPrimary by mutableStateOf(iconPrimary)
        private set

    // Inactive tab
    var iconPrimaryInactive by mutableStateOf(iconPrimaryInactive)
        private set

    // Secondary icon
    var iconSecondary by mutableStateOf(iconSecondary)
        private set

    // Active tab
    var iconActive by mutableStateOf(iconActive)
        private set

    // Disabled icon
    var iconDisabled by mutableStateOf(iconDisabled)
        private set

    // Icon inverted (on color)
    var iconOnColor by mutableStateOf(iconOnColor)
        private set

    // New
    var iconNotice by mutableStateOf(iconNotice)
        private set

    // Icon button
    var iconButton by mutableStateOf(iconButton)
        private set
    var iconWarning by mutableStateOf(iconWarning)
        private set

    // Warning icon on Secondary button
    var iconWarningButton by mutableStateOf(iconWarningButton)
        private set
    var iconAccentViolet by mutableStateOf(iconAccentViolet)
        private set
    var iconAccentBlue by mutableStateOf(iconAccentBlue)
        private set
    var iconAccentPink by mutableStateOf(iconAccentPink)
        private set
    var iconAccentGreen by mutableStateOf(iconAccentGreen)
        private set
    var iconAccentYellow by mutableStateOf(iconAccentYellow)
        private set

    // Action primary icon
    var iconActionPrimary by mutableStateOf(iconActionPrimary)
        private set

    // Action secondary icon
    var iconActionSecondary by mutableStateOf(iconActionSecondary)
        private set

    // Action tertiary icon
    var iconActionTertiary by mutableStateOf(iconActionTertiary)
        private set

    // Reader, ETP Shield
    var iconGradientStart by mutableStateOf(iconGradientStart)
        private set

    // Reader, ETP Shield
    var iconGradientEnd by mutableStateOf(iconGradientEnd)
        private set

    // Border

    // Default, Divider, Dotted
    var borderPrimary by mutableStateOf(borderPrimary)
        private set

    // Onboarding
    var borderInverted by mutableStateOf(borderInverted)
        private set

    // Form parts
    var borderFormDefault by mutableStateOf(borderFormDefault)
        private set

    // Active tab (Nav), Selected tab, Active form
    var borderAccent by mutableStateOf(borderAccent)
        private set

    // Form parts
    var borderDisabled by mutableStateOf(borderDisabled)
        private set

    // Form parts
    var borderWarning by mutableStateOf(borderWarning)
        private set

    fun update(other: FirefoxColors) {
        layer1 = other.layer1
        layer2 = other.layer2
        layer3 = other.layer3
        layer4Start = other.layer4Start
        layer4Center = other.layer4Center
        layer4End = other.layer4End
        layerAccent = other.layerAccent
        layerAccentNonOpaque = other.layerAccentNonOpaque
        layerAccentOpaque = other.layerAccentOpaque
        scrim = other.scrim
        gradientStart = other.gradientStart
        gradientEnd = other.gradientEnd
        layerWarning = other.layerWarning
        layerConfirmation = other.layerConfirmation
        layerError = other.layerError
        layerInfo = other.layerInfo
        actionPrimary = other.actionPrimary
        actionSecondary = other.actionSecondary
        actionTertiary = other.actionTertiary
        actionQuarternary = other.actionQuarternary
        actionWarning = other.actionWarning
        actionConfirmation = other.actionConfirmation
        actionError = other.actionError
        actionInfo = other.actionInfo
        formDefault = other.formDefault
        formSelected = other.formSelected
        formSurface = other.formSurface
        formDisabled = other.formDisabled
        formOn = other.formOn
        formOff = other.formOff
        indicatorActive = other.indicatorActive
        indicatorInactive = other.indicatorInactive
        textPrimary = other.textPrimary
        textSecondary = other.textSecondary
        textDisabled = other.textDisabled
        textWarning = other.textWarning
        textWarningButton = other.textWarningButton
        textAccent = other.textAccent
        textAccentDisabled = other.textAccentDisabled
        textOnColorPrimary = other.textOnColorPrimary
        textOnColorSecondary = other.textOnColorSecondary
        textActionPrimary = other.textActionPrimary
        textActionSecondary = other.textActionSecondary
        textActionTertiary = other.textActionTertiary
        textActionTertiaryActive = other.textActionTertiaryActive
        iconPrimary = other.iconPrimary
        iconPrimaryInactive = other.iconPrimaryInactive
        iconSecondary = other.iconSecondary
        iconActive = other.iconActive
        iconDisabled = other.iconDisabled
        iconOnColor = other.iconOnColor
        iconNotice = other.iconNotice
        iconButton = other.iconButton
        iconWarning = other.iconWarning
        iconWarningButton = other.iconWarningButton
        iconAccentViolet = other.iconAccentViolet
        iconAccentBlue = other.iconAccentBlue
        iconAccentPink = other.iconAccentPink
        iconAccentGreen = other.iconAccentGreen
        iconAccentYellow = other.iconAccentYellow
        iconActionPrimary = other.iconActionPrimary
        iconActionSecondary = other.iconActionSecondary
        iconActionTertiary = other.iconActionTertiary
        iconGradientStart = other.iconGradientStart
        iconGradientEnd = other.iconGradientEnd
        borderPrimary = other.borderPrimary
        borderInverted = other.borderInverted
        borderFormDefault = other.borderFormDefault
        borderAccent = other.borderAccent
        borderDisabled = other.borderDisabled
        borderWarning = other.borderWarning
    }

    /**
     * Return a copy of this [FirefoxColors] and optionally overriding any of the provided values.
     */
    fun copy(
        layer1: Color = this.layer1,
        layer2: Color = this.layer2,
        layer3: Color = this.layer3,
        layer4Start: Color = this.layer4Start,
        layer4Center: Color = this.layer4Center,
        layer4End: Color = this.layer4End,
        layerAccent: Color = this.layerAccent,
        layerAccentNonOpaque: Color = this.layerAccentNonOpaque,
        layerAccentOpaque: Color = this.layerAccentOpaque,
        scrim: Color = this.scrim,
        gradientStart: Color = this.gradientStart,
        gradientEnd: Color = this.gradientEnd,
        layerWarning: Color = this.layerWarning,
        layerConfirmation: Color = this.layerConfirmation,
        layerError: Color = this.layerError,
        layerInfo: Color = this.layerInfo,
        actionPrimary: Color = this.actionPrimary,
        actionSecondary: Color = this.actionSecondary,
        actionTertiary: Color = this.actionTertiary,
        actionQuarternary: Color = this.actionQuarternary,
        actionWarning: Color = this.actionWarning,
        actionConfirmation: Color = this.actionConfirmation,
        actionError: Color = this.actionError,
        actionInfo: Color = this.actionInfo,
        formDefault: Color = this.formDefault,
        formSelected: Color = this.formSelected,
        formSurface: Color = this.formSurface,
        formDisabled: Color = this.formDisabled,
        formOn: Color = this.formOn,
        formOff: Color = this.formOff,
        indicatorActive: Color = this.indicatorActive,
        indicatorInactive: Color = this.indicatorInactive,
        textPrimary: Color = this.textPrimary,
        textSecondary: Color = this.textSecondary,
        textDisabled: Color = this.textDisabled,
        textWarning: Color = this.textWarning,
        textWarningButton: Color = this.textWarningButton,
        textAccent: Color = this.textAccent,
        textAccentDisabled: Color = this.textAccentDisabled,
        textOnColorPrimary: Color = this.textOnColorPrimary,
        textOnColorSecondary: Color = this.textOnColorSecondary,
        textActionPrimary: Color = this.textActionPrimary,
        textActionSecondary: Color = this.textActionSecondary,
        textActionTertiary: Color = this.textActionTertiary,
        textActionTertiaryActive: Color = this.textActionTertiaryActive,
        iconPrimary: Color = this.iconPrimary,
        iconPrimaryInactive: Color = this.iconPrimaryInactive,
        iconSecondary: Color = this.iconSecondary,
        iconActive: Color = this.iconActive,
        iconDisabled: Color = this.iconDisabled,
        iconOnColor: Color = this.iconOnColor,
        iconNotice: Color = this.iconNotice,
        iconButton: Color = this.iconButton,
        iconWarning: Color = this.iconWarning,
        iconWarningButton: Color = this.iconWarningButton,
        iconAccentViolet: Color = this.iconAccentViolet,
        iconAccentBlue: Color = this.iconAccentBlue,
        iconAccentPink: Color = this.iconAccentPink,
        iconAccentGreen: Color = this.iconAccentGreen,
        iconAccentYellow: Color = this.iconAccentYellow,
        iconActionPrimary: Color = this.iconActionPrimary,
        iconActionSecondary: Color = this.iconActionSecondary,
        iconActionTertiary: Color = this.iconActionTertiary,
        iconGradientStart: Color = this.iconGradientStart,
        iconGradientEnd: Color = this.iconGradientEnd,
        borderPrimary: Color = this.borderPrimary,
        borderInverted: Color = this.borderInverted,
        borderFormDefault: Color = this.borderFormDefault,
        borderAccent: Color = this.borderAccent,
        borderDisabled: Color = this.borderDisabled,
        borderWarning: Color = this.borderWarning,
    ): FirefoxColors = FirefoxColors(
        layer1 = layer1,
        layer2 = layer2,
        layer3 = layer3,
        layer4Start = layer4Start,
        layer4Center = layer4Center,
        layer4End = layer4End,
        layerAccent = layerAccent,
        layerAccentNonOpaque = layerAccentNonOpaque,
        layerAccentOpaque = layerAccentOpaque,
        scrim = scrim,
        gradientStart = gradientStart,
        gradientEnd = gradientEnd,
        layerWarning = layerWarning,
        layerConfirmation = layerConfirmation,
        layerError = layerError,
        layerInfo = layerInfo,
        actionPrimary = actionPrimary,
        actionSecondary = actionSecondary,
        actionTertiary = actionTertiary,
        actionQuarternary = actionQuarternary,
        actionWarning = actionWarning,
        actionConfirmation = actionConfirmation,
        actionError = actionError,
        actionInfo = actionInfo,
        formDefault = formDefault,
        formSelected = formSelected,
        formSurface = formSurface,
        formDisabled = formDisabled,
        formOn = formOn,
        formOff = formOff,
        indicatorActive = indicatorActive,
        indicatorInactive = indicatorInactive,
        textPrimary = textPrimary,
        textSecondary = textSecondary,
        textDisabled = textDisabled,
        textWarning = textWarning,
        textWarningButton = textWarningButton,
        textAccent = textAccent,
        textAccentDisabled = textAccentDisabled,
        textOnColorPrimary = textOnColorPrimary,
        textOnColorSecondary = textOnColorSecondary,
        textActionPrimary = textActionPrimary,
        textActionSecondary = textActionSecondary,
        textActionTertiary = textActionTertiary,
        textActionTertiaryActive = textActionTertiaryActive,
        iconPrimary = iconPrimary,
        iconPrimaryInactive = iconPrimaryInactive,
        iconSecondary = iconSecondary,
        iconActive = iconActive,
        iconDisabled = iconDisabled,
        iconOnColor = iconOnColor,
        iconNotice = iconNotice,
        iconButton = iconButton,
        iconWarning = iconWarning,
        iconWarningButton = iconWarningButton,
        iconAccentViolet = iconAccentViolet,
        iconAccentBlue = iconAccentBlue,
        iconAccentPink = iconAccentPink,
        iconAccentGreen = iconAccentGreen,
        iconAccentYellow = iconAccentYellow,
        iconActionPrimary = iconActionPrimary,
        iconActionSecondary = iconActionSecondary,
        iconActionTertiary = iconActionTertiary,
        iconGradientStart = iconGradientStart,
        iconGradientEnd = iconGradientEnd,
        borderPrimary = borderPrimary,
        borderInverted = borderInverted,
        borderFormDefault = borderFormDefault,
        borderAccent = borderAccent,
        borderDisabled = borderDisabled,
        borderWarning = borderWarning,
    )
}

@Composable
fun ProvideFirefoxColors(
    colors: FirefoxColors,
    content: @Composable () -> Unit,
) {
    val colorPalette = remember {
        // Explicitly creating a new object here so we don't mutate the initial [colors]
        // provided, and overwrite the values set in it.
        colors.copy()
    }
    colorPalette.update(colors)
    CompositionLocalProvider(localFirefoxColors provides colorPalette, content = content)
}

private val localFirefoxColors = staticCompositionLocalOf<FirefoxColors> {
    error("No FirefoxColors provided")
}
