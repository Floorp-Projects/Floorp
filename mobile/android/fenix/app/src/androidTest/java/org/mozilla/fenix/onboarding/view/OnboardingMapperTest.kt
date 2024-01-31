/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

import io.mockk.every
import io.mockk.mockk
import mozilla.components.service.nimbus.evalJexlSafe
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.experiments.nimbus.NimbusMessagingHelperInterface
import org.mozilla.experiments.nimbus.StringHolder
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.nimbus.FxNimbus
import org.mozilla.fenix.nimbus.JunoOnboarding
import org.mozilla.fenix.nimbus.OnboardingCardData
import org.mozilla.fenix.nimbus.OnboardingCardType

class OnboardingMapperTest {

    @get:Rule
    val activityTestRule =
        HomeActivityIntentTestRule.withDefaultSettingsOverrides(skipOnboarding = true)

    private lateinit var junoOnboardingFeature: JunoOnboarding
    private lateinit var jexlConditions: Map<String, String>
    private lateinit var jexlHelper: NimbusMessagingHelperInterface
    private lateinit var evalFunction: (String) -> Boolean

    @Before
    fun setup() {
        junoOnboardingFeature = FxNimbus.features.junoOnboarding.value()
        jexlConditions = junoOnboardingFeature.conditions

        jexlHelper = mockk(relaxed = true)
        evalFunction = { condition -> jexlHelper.evalJexlSafe(condition) }

        every { evalFunction("true") } returns true
        every { evalFunction("false") } returns false
    }

    @Test
    fun showNotificationTrue_showAddWidgetFalse_pagesToDisplay_returnsSortedListOfAllConvertedPages_withoutAddWidgetPage() {
        val expected = listOf(defaultBrowserPageUiDataWithPrivacyCaption, syncPageUiData, notificationPageUiData)
        assertEquals(
            expected,
            unsortedAllKnownCardData.toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = true,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun showNotificationFalse_showAddWidgetFalse_pagesToDisplay_returnsSortedListOfConvertedPages_withoutNotificationPage_and_addWidgetPage() {
        val expected = listOf(defaultBrowserPageUiDataWithPrivacyCaption, syncPageUiData)
        assertEquals(
            expected,
            unsortedAllKnownCardData.toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun pagesToDisplay_returnsSortedListOfConvertedPages_withPrivacyCaption_alwaysOnFirstPage() {
        var result = unsortedAllKnownCardData.toPageUiData(
            privacyCaption = privacyCaption,
            showDefaultBrowserPage = false,
            showNotificationPage = false,
            showAddWidgetPage = false,
            jexlConditions = jexlConditions,
            func = evalFunction,
        )
        assertEquals(result[0].privacyCaption, privacyCaption)

        result = unsortedAllKnownCardData.toPageUiData(
            privacyCaption = privacyCaption,
            showDefaultBrowserPage = false,
            showNotificationPage = true,
            showAddWidgetPage = false,
            jexlConditions = jexlConditions,
            func = evalFunction,
        )
        assertEquals(result[0].privacyCaption, privacyCaption)
        assertEquals(result[1].privacyCaption, null)

        result = unsortedAllKnownCardData.toPageUiData(
            privacyCaption = privacyCaption,
            showDefaultBrowserPage = true,
            showNotificationPage = true,
            showAddWidgetPage = false,
            jexlConditions = jexlConditions,
            func = evalFunction,
        )
        assertEquals(result[0].privacyCaption, privacyCaption)
        assertEquals(result[1].privacyCaption, null)
        assertEquals(result[2].privacyCaption, null)

        result = unsortedAllKnownCardData.toPageUiData(
            privacyCaption = privacyCaption,
            showDefaultBrowserPage = false,
            showNotificationPage = false,
            showAddWidgetPage = true,
            jexlConditions = jexlConditions,
            func = evalFunction,
        )
        assertEquals(result[0].privacyCaption, privacyCaption)
        assertEquals(result[1].privacyCaption, null)
    }

    @Test
    fun showDefaultBrowserPageFalse_showNotificationFalse_showAddWidgetTrue_pagesToDisplay_returnsSortedListOfAllConvertedPages() {
        val expected = listOf(addSearchWidgetPageUiDataWithPrivacyCaption, syncPageUiData)
        assertEquals(
            expected,
            unsortedAllKnownCardData.toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = false,
                showNotificationPage = false,
                showAddWidgetPage = true,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun showNotificationFalse_showAddWidgetTrue_pagesToDisplay_returnsSortedListOfAllConvertedPages_withoutNotificationPage() {
        val expected = listOf(defaultBrowserPageUiDataWithPrivacyCaption, addSearchWidgetPageUiData, syncPageUiData)
        assertEquals(
            expected,
            unsortedAllKnownCardData.toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = true,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun showNotificationTrue_and_showAddWidgetTrue_pagesToDisplay_returnsSortedListOfConvertedPages() {
        val expected = listOf(
            defaultBrowserPageUiDataWithPrivacyCaption,
            addSearchWidgetPageUiData,
            syncPageUiData,
            notificationPageUiData,
        )
        assertEquals(
            expected,
            unsortedAllKnownCardData.toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = true,
                showAddWidgetPage = true,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun cardConditionsMatchJexlConditions_shouldDisplayCard_returnsConvertedPage() {
        val jexlConditions = mapOf("ALWAYS" to "true", "NEVER" to "false")
        val expected = listOf(defaultBrowserPageUiDataWithPrivacyCaption)

        assertEquals(
            expected,
            listOf(defaultBrowserCardData).toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun noJexlConditionsAndNoCardConditions_shouldDisplayCard_returnsNoPage() {
        val jexlConditions = mapOf<String, String>()
        val expected = emptyList<OnboardingPageUiData>()

        assertEquals(
            expected,
            listOf(addSearchWidgetCardDataNoConditions).toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun noJexlConditions_shouldDisplayCard_returnsNoPage() {
        val jexlConditions = mapOf<String, String>()
        val expected = emptyList<OnboardingPageUiData>()

        assertEquals(
            expected,
            listOf(defaultBrowserCardData).toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun prerequisitesMatchJexlConditions_shouldDisplayCard_returnsConvertedPage() {
        val jexlConditions = mapOf("ALWAYS" to "true")
        val expected = listOf(defaultBrowserPageUiDataWithPrivacyCaption)

        assertEquals(
            expected,
            listOf(defaultBrowserCardData).toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun prerequisitesDontMatchJexlConditions_shouldDisplayCard_returnsNoPage() {
        val jexlConditions = mapOf("NEVER" to "false")
        val expected = emptyList<OnboardingPageUiData>()

        assertEquals(
            expected,
            listOf(defaultBrowserCardData).toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun noCardConditions_shouldDisplayCard_returnsNoPage() {
        val jexlConditions = mapOf("ALWAYS" to "true", "NEVER" to "false")
        val expected = emptyList<OnboardingPageUiData>()

        assertEquals(
            expected,
            listOf(addSearchWidgetCardDataNoConditions).toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun noDisqualifiers_shouldDisplayCard_returnsConvertedPage() {
        val jexlConditions = mapOf("ALWAYS" to "true", "NEVER" to "false")
        val expected = listOf(defaultBrowserPageUiDataWithPrivacyCaption)

        assertEquals(
            expected,
            listOf(defaultBrowserCardDataNoDisqualifiers).toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun disqualifiersMatchJexlConditions_shouldDisplayCard_returnsConvertedPage() {
        val jexlConditions = mapOf("NEVER" to "false")
        val expected = listOf(syncPageUiDataWithPrivacyCaption)

        assertEquals(
            expected,
            listOf(syncCardData).toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun disqualifiersDontMatchJexlConditions_shouldDisplayCard_returnsNoPage() {
        val jexlConditions = mapOf("NEVER" to "false")
        val expected = listOf<OnboardingPageUiData>()

        assertEquals(
            expected,
            listOf(notificationCardData).toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun noPrerequisites_shouldDisplayCard_returnsConvertedPage() {
        val jexlConditions = mapOf("ALWAYS" to "true", "NEVER" to "false")
        val expected = listOf(syncPageUiDataWithPrivacyCaption)

        assertEquals(
            expected,
            listOf(syncCardData).toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }
}
val privacyCaption: Caption = mockk(relaxed = true)

private val defaultBrowserPageUiDataWithPrivacyCaption = OnboardingPageUiData(
    type = OnboardingPageUiData.Type.DEFAULT_BROWSER,
    imageRes = R.drawable.ic_onboarding_welcome,
    title = "default browser title",
    description = "default browser body",
    primaryButtonLabel = "default browser primary button text",
    secondaryButtonLabel = "default browser secondary button text",
    privacyCaption = privacyCaption,
)
private val addSearchWidgetPageUiData = OnboardingPageUiData(
    type = OnboardingPageUiData.Type.ADD_SEARCH_WIDGET,
    imageRes = R.drawable.ic_onboarding_search_widget,
    title = "add search widget title",
    description = "add search widget body",
    primaryButtonLabel = "add search widget primary button text",
    secondaryButtonLabel = "add search widget secondary button text",
    privacyCaption = null,
)
private val addSearchWidgetPageUiDataWithPrivacyCaption = OnboardingPageUiData(
    type = OnboardingPageUiData.Type.ADD_SEARCH_WIDGET,
    imageRes = R.drawable.ic_onboarding_search_widget,
    title = "add search widget title",
    description = "add search widget body",
    primaryButtonLabel = "add search widget primary button text",
    secondaryButtonLabel = "add search widget secondary button text",
    privacyCaption = privacyCaption,
)
private val syncPageUiData = OnboardingPageUiData(
    type = OnboardingPageUiData.Type.SYNC_SIGN_IN,
    imageRes = R.drawable.ic_onboarding_sync,
    title = "sync title",
    description = "sync body",
    primaryButtonLabel = "sync primary button text",
    secondaryButtonLabel = "sync secondary button text",
    privacyCaption = null,
)
private val syncPageUiDataWithPrivacyCaption = OnboardingPageUiData(
    type = OnboardingPageUiData.Type.SYNC_SIGN_IN,
    imageRes = R.drawable.ic_onboarding_sync,
    title = "sync title",
    description = "sync body",
    primaryButtonLabel = "sync primary button text",
    secondaryButtonLabel = "sync secondary button text",
    privacyCaption = privacyCaption,
)
private val notificationPageUiData = OnboardingPageUiData(
    type = OnboardingPageUiData.Type.NOTIFICATION_PERMISSION,
    imageRes = R.drawable.ic_notification_permission,
    title = "notification title",
    description = "notification body",
    primaryButtonLabel = "notification primary button text",
    secondaryButtonLabel = "notification secondary button text",
    privacyCaption = null,
)

private val defaultBrowserCardData = OnboardingCardData(
    cardType = OnboardingCardType.DEFAULT_BROWSER,
    imageRes = R.drawable.ic_onboarding_welcome,
    title = StringHolder(null, "default browser title"),
    body = StringHolder(null, "default browser body"),
    primaryButtonLabel = StringHolder(null, "default browser primary button text"),
    secondaryButtonLabel = StringHolder(null, "default browser secondary button text"),
    ordering = 10,
    prerequisites = listOf("ALWAYS"),
    disqualifiers = listOf("NEVER"),
)

private val defaultBrowserCardDataNoDisqualifiers = OnboardingCardData(
    cardType = OnboardingCardType.DEFAULT_BROWSER,
    imageRes = R.drawable.ic_onboarding_welcome,
    title = StringHolder(null, "default browser title"),
    body = StringHolder(null, "default browser body"),
    primaryButtonLabel = StringHolder(null, "default browser primary button text"),
    secondaryButtonLabel = StringHolder(null, "default browser secondary button text"),
    ordering = 10,
    prerequisites = listOf("ALWAYS"),
    disqualifiers = listOf(),
)

private val addSearchWidgetCardDataNoConditions = OnboardingCardData(
    cardType = OnboardingCardType.ADD_SEARCH_WIDGET,
    imageRes = R.drawable.ic_onboarding_search_widget,
    title = StringHolder(null, "add search widget title"),
    body = StringHolder(null, "add search widget body"),
    primaryButtonLabel = StringHolder(null, "add search widget primary button text"),
    secondaryButtonLabel = StringHolder(null, "add search widget secondary button text"),
    ordering = 15,
    prerequisites = listOf(),
    disqualifiers = listOf(),
)

private val addSearchWidgetCardData = OnboardingCardData(
    cardType = OnboardingCardType.ADD_SEARCH_WIDGET,
    imageRes = R.drawable.ic_onboarding_search_widget,
    title = StringHolder(null, "add search widget title"),
    body = StringHolder(null, "add search widget body"),
    primaryButtonLabel = StringHolder(null, "add search widget primary button text"),
    secondaryButtonLabel = StringHolder(null, "add search widget secondary button text"),
    ordering = 15,
)

private val syncCardData = OnboardingCardData(
    cardType = OnboardingCardType.SYNC_SIGN_IN,
    imageRes = R.drawable.ic_onboarding_sync,
    title = StringHolder(null, "sync title"),
    body = StringHolder(null, "sync body"),
    primaryButtonLabel = StringHolder(null, "sync primary button text"),
    secondaryButtonLabel = StringHolder(null, "sync secondary button text"),
    ordering = 20,
    prerequisites = listOf(),
    disqualifiers = listOf("NEVER"),
)

private val notificationCardData = OnboardingCardData(
    cardType = OnboardingCardType.NOTIFICATION_PERMISSION,
    imageRes = R.drawable.ic_notification_permission,
    title = StringHolder(null, "notification title"),
    body = StringHolder(null, "notification body"),
    primaryButtonLabel = StringHolder(null, "notification primary button text"),
    secondaryButtonLabel = StringHolder(null, "notification secondary button text"),
    ordering = 30,
    prerequisites = listOf(),
    disqualifiers = listOf("NEVER", "OTHER"),
)

private val unsortedAllKnownCardData = listOf(
    syncCardData,
    notificationCardData,
    defaultBrowserCardData,
    addSearchWidgetCardData,
)
