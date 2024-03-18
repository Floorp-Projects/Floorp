/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

import org.mozilla.fenix.nimbus.OnboardingCardData
import org.mozilla.fenix.nimbus.OnboardingCardType

/**
 * Returns a list of all the required Nimbus 'cards' that have been converted to [OnboardingPageUiData].
 */
internal fun Collection<OnboardingCardData>.toPageUiData(
    privacyCaption: Caption,
    showDefaultBrowserPage: Boolean,
    showNotificationPage: Boolean,
    showAddWidgetPage: Boolean,
    jexlConditions: Map<String, String>,
    func: (String) -> Boolean,
): List<OnboardingPageUiData> {
    // we are first filtering the cards based on Nimbus configuration
    return filter { it.shouldDisplayCard(func, jexlConditions) }
        // we are then filtering again based on device capabilities
        .filter { it.isCardEnabled(showDefaultBrowserPage, showNotificationPage, showAddWidgetPage) }
        .sortedBy { it.ordering }
        .mapIndexed {
                index, onboardingCardData ->
            // only first onboarding card shows privacy caption
            onboardingCardData.toPageUiData(if (index == 0) privacyCaption else null)
        }
}

private fun OnboardingCardData.isCardEnabled(
    showDefaultBrowserPage: Boolean,
    showNotificationPage: Boolean,
    showAddWidgetPage: Boolean,
): Boolean =
    when (cardType) {
        OnboardingCardType.DEFAULT_BROWSER -> {
            enabled && showDefaultBrowserPage
        }

        OnboardingCardType.NOTIFICATION_PERMISSION -> {
            enabled && showNotificationPage
        }

        OnboardingCardType.ADD_SEARCH_WIDGET -> {
            enabled && showAddWidgetPage
        }

        else -> {
            enabled
        }
    }

/**
 *  Determines whether the given [OnboardingCardData] should be displayed.
 *
 *  @param func Function that receives a condition as a [String] and returns its JEXL evaluation as a [Boolean].
 *  @param jexlConditions A <String, String> map containing the Nimbus conditions.
 *
 *  @return True if the card should be displayed, otherwise false.
 */
private fun OnboardingCardData.shouldDisplayCard(
    func: (String) -> Boolean,
    jexlConditions: Map<String, String>,
): Boolean {
    val jexlCache: MutableMap<String, Boolean> = mutableMapOf()

    // Make sure the conditions exist and have a value, and that the number
    // of valid conditions matches the number of conditions on the card's
    // respective prerequisite or disqualifier table. If these mismatch,
    // that means a card contains a condition that's not in the feature
    // conditions lookup table. JEXLs can only be evaluated on
    // supported conditions. Otherwise, consider the card invalid.
    val allPrerequisites = prerequisites.mapNotNull { jexlConditions[it] }
    val allDisqualifiers = disqualifiers.mapNotNull { jexlConditions[it] }

    val validPrerequisites = if (allPrerequisites.size == prerequisites.size) {
        allPrerequisites.all { condition ->
            jexlCache.getOrPut(condition) {
                func(condition)
            }
        }
    } else {
        false
    }

    val hasDisqualifiers =
        if (allDisqualifiers.isNotEmpty() && allDisqualifiers.size == disqualifiers.size) {
            allDisqualifiers.all { condition ->
                jexlCache.getOrPut(condition) {
                    func(condition)
                }
            }
        } else {
            false
        }

    return validPrerequisites && !hasDisqualifiers
}

private fun OnboardingCardData.toPageUiData(privacyCaption: Caption?) = OnboardingPageUiData(
    type = cardType.toPageUiDataType(),
    imageRes = imageRes.resourceId,
    title = title,
    description = body,
    primaryButtonLabel = primaryButtonLabel,
    secondaryButtonLabel = secondaryButtonLabel,
    privacyCaption = privacyCaption,
)

private fun OnboardingCardType.toPageUiDataType() = when (this) {
    OnboardingCardType.DEFAULT_BROWSER -> OnboardingPageUiData.Type.DEFAULT_BROWSER
    OnboardingCardType.SYNC_SIGN_IN -> OnboardingPageUiData.Type.SYNC_SIGN_IN
    OnboardingCardType.NOTIFICATION_PERMISSION -> OnboardingPageUiData.Type.NOTIFICATION_PERMISSION
    OnboardingCardType.ADD_SEARCH_WIDGET -> OnboardingPageUiData.Type.ADD_SEARCH_WIDGET
}

/**
 * Mapper to convert [OnboardingPageUiData] to [OnboardingPageState] that is a param for
 * [OnboardingPage] composable.
 */
@Suppress("LongParameterList")
internal fun mapToOnboardingPageState(
    onboardingPageUiData: OnboardingPageUiData,
    onMakeFirefoxDefaultClick: () -> Unit,
    onMakeFirefoxDefaultSkipClick: () -> Unit,
    onSignInButtonClick: () -> Unit,
    onSignInSkipClick: () -> Unit,
    onNotificationPermissionButtonClick: () -> Unit,
    onNotificationPermissionSkipClick: () -> Unit,
    onAddFirefoxWidgetClick: () -> Unit,
    onAddFirefoxWidgetSkipClick: () -> Unit,
): OnboardingPageState = when (onboardingPageUiData.type) {
    OnboardingPageUiData.Type.DEFAULT_BROWSER -> createOnboardingPageState(
        onboardingPageUiData = onboardingPageUiData,
        onPositiveButtonClick = onMakeFirefoxDefaultClick,
        onNegativeButtonClick = onMakeFirefoxDefaultSkipClick,
    )

    OnboardingPageUiData.Type.ADD_SEARCH_WIDGET -> createOnboardingPageState(
        onboardingPageUiData = onboardingPageUiData,
        onPositiveButtonClick = onAddFirefoxWidgetClick,
        onNegativeButtonClick = onAddFirefoxWidgetSkipClick,
    )

    OnboardingPageUiData.Type.SYNC_SIGN_IN -> createOnboardingPageState(
        onboardingPageUiData = onboardingPageUiData,
        onPositiveButtonClick = onSignInButtonClick,
        onNegativeButtonClick = onSignInSkipClick,
    )

    OnboardingPageUiData.Type.NOTIFICATION_PERMISSION -> createOnboardingPageState(
        onboardingPageUiData = onboardingPageUiData,
        onPositiveButtonClick = onNotificationPermissionButtonClick,
        onNegativeButtonClick = onNotificationPermissionSkipClick,
    )
}

private fun createOnboardingPageState(
    onboardingPageUiData: OnboardingPageUiData,
    onPositiveButtonClick: () -> Unit,
    onNegativeButtonClick: () -> Unit,
): OnboardingPageState = OnboardingPageState(
    imageRes = onboardingPageUiData.imageRes,
    title = onboardingPageUiData.title,
    description = onboardingPageUiData.description,
    primaryButton = Action(onboardingPageUiData.primaryButtonLabel, onPositiveButtonClick),
    secondaryButton = Action(onboardingPageUiData.secondaryButtonLabel, onNegativeButtonClick),
    privacyCaption = onboardingPageUiData.privacyCaption,
)
