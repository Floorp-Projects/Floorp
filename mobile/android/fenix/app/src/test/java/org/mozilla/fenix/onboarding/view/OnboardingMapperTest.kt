/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.R

class OnboardingMapperTest {

    @Test
    fun `GIVEN a default browser page WHEN mapToOnboardingPageState is called THEN creates the expected OnboardingPageState`() {
        val expected = OnboardingPageState(
            imageRes = R.drawable.ic_onboarding_welcome,
            title = "default browser title",
            description = "default browser body with link text",
            primaryButton = Action("default browser primary button text", unitLambda),
            secondaryButton = Action("default browser secondary button text", unitLambda),
        )

        val onboardingPageUiData = OnboardingPageUiData(
            type = OnboardingPageUiData.Type.DEFAULT_BROWSER,
            imageRes = R.drawable.ic_onboarding_welcome,
            title = "default browser title",
            description = "default browser body with link text",
            primaryButtonLabel = "default browser primary button text",
            secondaryButtonLabel = "default browser secondary button text",
            privacyCaption = null,
        )
        val actual = mapToOnboardingPageState(
            onboardingPageUiData = onboardingPageUiData,
            onMakeFirefoxDefaultClick = unitLambda,
            onMakeFirefoxDefaultSkipClick = unitLambda,
            onSignInButtonClick = {},
            onSignInSkipClick = {},
            onNotificationPermissionButtonClick = {},
            onNotificationPermissionSkipClick = {},
            onAddFirefoxWidgetClick = {},
            onAddFirefoxWidgetSkipClick = {},
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `GIVEN a sync page WHEN mapToOnboardingPageState is called THEN creates the expected OnboardingPageState`() {
        val expected = OnboardingPageState(
            imageRes = R.drawable.ic_onboarding_sync,
            title = "sync title",
            description = "sync body",
            primaryButton = Action("sync primary button text", unitLambda),
            secondaryButton = Action("sync secondary button text", unitLambda),
        )

        val onboardingPageUiData = OnboardingPageUiData(
            type = OnboardingPageUiData.Type.SYNC_SIGN_IN,
            imageRes = R.drawable.ic_onboarding_sync,
            title = "sync title",
            description = "sync body",
            primaryButtonLabel = "sync primary button text",
            secondaryButtonLabel = "sync secondary button text",
            privacyCaption = null,
        )
        val actual = mapToOnboardingPageState(
            onboardingPageUiData = onboardingPageUiData,
            onMakeFirefoxDefaultClick = {},
            onMakeFirefoxDefaultSkipClick = {},
            onSignInButtonClick = unitLambda,
            onSignInSkipClick = unitLambda,
            onNotificationPermissionButtonClick = {},
            onNotificationPermissionSkipClick = {},
            onAddFirefoxWidgetClick = {},
            onAddFirefoxWidgetSkipClick = {},
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `GIVEN a notification page WHEN mapToOnboardingPageState is called THEN creates the expected OnboardingPageState`() {
        val expected = OnboardingPageState(
            imageRes = R.drawable.ic_notification_permission,
            title = "notification title",
            description = "notification body",
            primaryButton = Action("notification primary button text", unitLambda),
            secondaryButton = Action("notification secondary button text", unitLambda),
        )

        val onboardingPageUiData = OnboardingPageUiData(
            type = OnboardingPageUiData.Type.NOTIFICATION_PERMISSION,
            imageRes = R.drawable.ic_notification_permission,
            title = "notification title",
            description = "notification body",
            primaryButtonLabel = "notification primary button text",
            secondaryButtonLabel = "notification secondary button text",
            privacyCaption = null,
        )
        val actual = mapToOnboardingPageState(
            onboardingPageUiData = onboardingPageUiData,
            onMakeFirefoxDefaultClick = {},
            onMakeFirefoxDefaultSkipClick = {},
            onSignInButtonClick = {},
            onSignInSkipClick = {},
            onNotificationPermissionButtonClick = unitLambda,
            onNotificationPermissionSkipClick = unitLambda,
            onAddFirefoxWidgetClick = {},
            onAddFirefoxWidgetSkipClick = {},
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `GIVEN an add search widget page WHEN mapToOnboardingPageState is called THEN creates the expected OnboardingPageState`() {
        val expected = OnboardingPageState(
            imageRes = R.drawable.ic_onboarding_search_widget,
            title = "add search widget title",
            description = "add search widget body with link text",
            primaryButton = Action("add search widget primary button text", unitLambda),
            secondaryButton = Action("add search widget secondary button text", unitLambda),
        )

        val onboardingPageUiData = OnboardingPageUiData(
            type = OnboardingPageUiData.Type.ADD_SEARCH_WIDGET,
            imageRes = R.drawable.ic_onboarding_search_widget,
            title = "add search widget title",
            description = "add search widget body with link text",
            primaryButtonLabel = "add search widget primary button text",
            secondaryButtonLabel = "add search widget secondary button text",
            privacyCaption = null,
        )
        val actual = mapToOnboardingPageState(
            onboardingPageUiData = onboardingPageUiData,
            onMakeFirefoxDefaultClick = {},
            onMakeFirefoxDefaultSkipClick = {},
            onSignInButtonClick = {},
            onSignInSkipClick = {},
            onNotificationPermissionButtonClick = {},
            onNotificationPermissionSkipClick = {},
            onAddFirefoxWidgetClick = unitLambda,
            onAddFirefoxWidgetSkipClick = unitLambda,
        )

        assertEquals(expected, actual)
    }
}

private val unitLambda = { dummyUnitFunc() }

private fun dummyUnitFunc() {}

private fun dummyStringArgFunc(string: String) {
    print(string)
}
