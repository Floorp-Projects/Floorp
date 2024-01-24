/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.AnimatedVisibilityScope
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.tween
import androidx.compose.animation.expandIn
import androidx.compose.animation.fadeIn
import androidx.compose.animation.slideInHorizontally
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Divider
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.rememberNestedScrollInteropConnection
import androidx.compose.ui.unit.Density
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.dp
import mozilla.components.concept.engine.translate.Language
import mozilla.components.concept.engine.translate.TranslationError
import org.mozilla.fenix.theme.FirefoxTheme

private const val BOTTOM_SHEET_HANDLE_WIDTH_PERCENT = 0.1f

@Composable
internal fun TranslationDialogBottomSheet(content: @Composable () -> Unit) {
    Column(
        modifier = Modifier
            .background(
                color = FirefoxTheme.colors.layer2,
                shape = RoundedCornerShape(topStart = 8.dp, topEnd = 8.dp),
            )
            .nestedScroll(rememberNestedScrollInteropConnection()),
    ) {
        Divider(
            Modifier
                .padding(top = 16.dp)
                .fillMaxWidth(BOTTOM_SHEET_HANDLE_WIDTH_PERCENT)
                .align(alignment = Alignment.CenterHorizontally),
            color = FirefoxTheme.colors.borderInverted,
            thickness = 3.dp,
        )

        content()
    }
}

@Composable
internal fun TranslationsAnimation(
    translationsVisibility: Boolean,
    density: Density,
    translationsOptionsHeightDp: Dp,
    content: @Composable AnimatedVisibilityScope.() -> Unit,
) {
    AnimatedVisibility(
        visible = translationsVisibility,
        enter = expandIn(
            animationSpec = tween(
                easing = FastOutSlowInEasing,
            ),
            initialSize = {
                with(density) {
                    IntSize(
                        0,
                        translationsOptionsHeightDp.roundToPx(),
                    )
                }
            },
        ) + fadeIn(
            animationSpec = tween(
                easing = FastOutSlowInEasing,
            ),
        ) + slideInHorizontally(
            animationSpec = tween(
                easing = FastOutSlowInEasing,
            ),
        ),
    ) {
        content()
    }
}

@Composable
internal fun TranslationsOptionsAnimation(
    translationsVisibility: Boolean,
    density: Density,
    translationsHeightDp: Dp,
    translationsWidthDp: Dp,
    content: @Composable AnimatedVisibilityScope.() -> Unit,
) {
    AnimatedVisibility(
        visible = translationsVisibility,
        enter = expandIn(
            animationSpec = tween(
                easing = FastOutSlowInEasing,
            ),
            initialSize = {
                with(density) {
                    IntSize(
                        0,
                        translationsHeightDp.roundToPx(),
                    )
                }
            },
        ) + fadeIn(
            animationSpec = tween(
                easing = FastOutSlowInEasing,
            ),
        ) + slideInHorizontally(
            initialOffsetX = { with(density) { translationsWidthDp.roundToPx() } },
            animationSpec = tween(
                easing = FastOutSlowInEasing,
            ),
        ),
    ) {
        content()
    }
}

@Composable
@Suppress("LongParameterList")
internal fun TranslationsDialog(
    learnMoreUrl: String,
    showFirstTimeTranslation: Boolean,
    translateFromLanguages: List<Language>?,
    translateToLanguages: List<Language>?,
    initialFrom: Language? = null,
    initialTo: Language? = null,
    translationError: TranslationError? = null,
    onSettingClicked: () -> Unit,
    onLearnMoreClicked: () -> Unit,
    onTranslateButtonClick: () -> Unit,
    onNotNowButtonClick: () -> Unit,
    onFromSelected: (Language) -> Unit,
    onToSelected: (Language) -> Unit,
) {
    TranslationsDialogBottomSheet(
        learnMoreUrl = learnMoreUrl,
        showFirstTimeTranslation = showFirstTimeTranslation,
        translationError = translationError,
        translateFromLanguages = translateFromLanguages,
        translateToLanguages = translateToLanguages,
        initialFrom = initialFrom,
        initialTo = initialTo,
        onSettingClicked = onSettingClicked,
        onLearnMoreClicked = onLearnMoreClicked,
        onTranslateButtonClicked = onTranslateButtonClick,
        onNotNowButtonClicked = onNotNowButtonClick,
        onFromSelected = onFromSelected,
        onToSelected = onToSelected,
    )
}

@Composable
internal fun TranslationsOptionsDialog(
    onBackClicked: () -> Unit,
    onTranslationSettingsClicked: () -> Unit,
) {
    TranslationOptionsDialog(
        translationOptionsList = getTranslationOptionsList(),
        onBackClicked = onBackClicked,
        onTranslationSettingsClicked = onTranslationSettingsClicked,
        aboutTranslationClicked = {},
    )
}
