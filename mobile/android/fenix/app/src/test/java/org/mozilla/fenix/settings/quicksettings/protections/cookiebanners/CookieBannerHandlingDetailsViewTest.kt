/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings.protections.cookiebanners

import android.view.View
import android.widget.FrameLayout
import io.mockk.MockKAnnotations
import io.mockk.coEvery
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.test.advanceUntilIdle
import mozilla.components.browser.state.state.createTab
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.ComponentCookieBannerDetailsPanelBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.trackingprotection.CookieBannerUIMode
import org.mozilla.fenix.trackingprotection.ProtectionsState

@RunWith(FenixRobolectricTestRunner::class)
class CookieBannerHandlingDetailsViewTest {

    private lateinit var view: CookieBannerHandlingDetailsView
    private lateinit var binding: ComponentCookieBannerDetailsPanelBinding
    private lateinit var interactor: CookieBannerDetailsInteractor

    @MockK(relaxed = true)
    private lateinit var publicSuffixList: PublicSuffixList

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        interactor = mockk(relaxed = true)
        view = spyk(
            CookieBannerHandlingDetailsView(
                container = FrameLayout(testContext),
                context = testContext,
                publicSuffixList = publicSuffixList,
                interactor = interactor,
                ioScope = scope,
                onDismiss = {},
            ),
        )
        binding = view.binding
    }

    @Test
    fun `WHEN updating THEN bind title,back button, description and switch`() {
        val websiteUrl = "https://mozilla.org"
        val state = ProtectionsState(
            tab = createTab(url = websiteUrl),
            url = websiteUrl,
            isTrackingProtectionEnabled = true,
            cookieBannerUIMode = CookieBannerUIMode.ENABLE,
            listTrackers = listOf(),
            mode = ProtectionsState.Mode.Normal,
            lastAccessedCategory = "",
        )

        view.update(state)

        verify {
            view.bindTitle(state.url, state.cookieBannerUIMode)
            view.bindBackButtonListener()
            view.bindDescription(state.cookieBannerUIMode)
            view.bindSwitch(state.cookieBannerUIMode)
        }
    }

    @Test
    fun `GIVEN cookie banner handling mode is enabled WHEN biding title THEN title view must have the expected string`() =
        runTestOnMain {
            coEvery { publicSuffixList.getPublicSuffixPlusOne(any()) } returns CompletableDeferred("mozilla.org")

            val websiteUrl = "https://mozilla.org"

            view.bindTitle(url = websiteUrl, state = CookieBannerUIMode.ENABLE)

            val expectedText =
                testContext.getString(
                    R.string.reduce_cookie_banner_details_panel_title_off_for_site,
                    "mozilla.org",
                )

            assertEquals(expectedText, view.binding.title.text)
        }

    @Test
    fun `GIVEN cookie banner handling mode is site not supported WHEN biding title THEN title view must have the expected string`() =
        runTestOnMain {
            coEvery { publicSuffixList.getPublicSuffixPlusOne(any()) } returns CompletableDeferred("mozilla.org")

            val websiteUrl = "https://mozilla.org"

            view.bindTitle(url = websiteUrl, state = CookieBannerUIMode.SITE_NOT_SUPPORTED)

            val expectedText =
                testContext.getString(
                    R.string.cookie_banner_handling_details_site_is_not_supported_title_2,
                )

            assertEquals(expectedText, view.binding.title.text)
        }

    @Test
    fun `GIVEN cookie banner handling mode is disabled WHEN biding title THEN title view must have the expected string`() =
        runTestOnMain {
            coEvery { publicSuffixList.getPublicSuffixPlusOne(any()) } returns CompletableDeferred("mozilla.org")

            val websiteUrl = "https://mozilla.org"

            view.bindTitle(
                url = websiteUrl,
                state = CookieBannerUIMode.DISABLE,
            )

            advanceUntilIdle()

            val expectedText =
                testContext.getString(
                    R.string.reduce_cookie_banner_details_panel_title_on_for_site,
                    "mozilla.org",
                )

            assertEquals(expectedText, view.binding.title.text)
        }

    @Test
    fun `WHEN clicking the back button THEN view must delegate to the interactor#onBackPressed()`() {
        view.bindBackButtonListener()

        view.binding.navigateBack.performClick()

        verify {
            interactor.onBackPressed()
        }
    }

    @Test
    fun `GIVEN cookie banner handling mode is enabled WHEN biding description THEN description view must have the expected string`() {
        view.bindDescription(state = CookieBannerUIMode.ENABLE)

        val expectedText =
            testContext.getString(
                R.string.reduce_cookie_banner_details_panel_description_off_for_site,
                testContext.getString(R.string.app_name),
            )

        assertEquals(expectedText, view.binding.details.text)
    }

    @Test
    fun `GIVEN cookie banner handling mode is site not supported WHEN biding description THEN description view must have the expected string`() {
        view.bindDescription(state = CookieBannerUIMode.SITE_NOT_SUPPORTED)

        val appName = testContext.getString(R.string.app_name)
        val expectedText =
            testContext.getString(
                R.string.reduce_cookie_banner_details_panel_title_unsupported_site_request_2,
                appName,
            )

        assertEquals(expectedText, view.binding.details.text)
    }

    @Test
    fun `GIVEN cookie banner handling mode is disabled WHEN biding description THEN description view must have the expected string`() {
        view.bindDescription(state = CookieBannerUIMode.DISABLE)

        val appName = testContext.getString(R.string.app_name)
        val expectedText =
            testContext.getString(
                R.string.reduce_cookie_banner_details_panel_description_on_for_site_2,
                appName,
                appName,
            )

        assertEquals(expectedText, view.binding.details.text)
    }

    @Test
    fun `GIVEN cookie banner handling is disabled WHEN biding switch THEN switch view must have the expected isChecked status`() {
        view.bindSwitch(state = CookieBannerUIMode.DISABLE)

        assertFalse(view.binding.cookieBannerSwitch.isChecked)
    }

    @Test
    fun `GIVEN cookie banner handling is enabled WHEN biding switch THEN switch view must have the expected isChecked status`() {
        view.bindSwitch(state = CookieBannerUIMode.ENABLE)

        assertTrue(view.binding.cookieBannerSwitch.isChecked)
    }

    @Test
    fun `GIVEN cookie banner handling is site not supported WHEN setUiForCookieBannerMode THEN set ui for site not supported should be visible`() {
        val websiteUrl = "https://mozilla.org"
        val state = ProtectionsState(
            tab = createTab(url = websiteUrl),
            url = websiteUrl,
            isTrackingProtectionEnabled = true,
            cookieBannerUIMode = CookieBannerUIMode.SITE_NOT_SUPPORTED,
            listTrackers = listOf(),
            mode = ProtectionsState.Mode.Normal,
            lastAccessedCategory = "",
        )

        view.update(state)

        assertEquals(View.GONE, view.binding.cookieBannerSwitch.visibility)
        assertEquals(View.VISIBLE, view.binding.cancelButton.visibility)
        assertEquals(View.VISIBLE, view.binding.requestSupport.visibility)
    }

    @Test
    fun `WHEN clicking the request support button THEN view must delegate to the interactor#handleRequestSiteSupportPressed()`() {
        val websiteUrl = "https://mozilla.org"
        val state = ProtectionsState(
            tab = createTab(url = websiteUrl),
            url = websiteUrl,
            isTrackingProtectionEnabled = true,
            cookieBannerUIMode = CookieBannerUIMode.SITE_NOT_SUPPORTED,
            listTrackers = listOf(),
            mode = ProtectionsState.Mode.Normal,
            lastAccessedCategory = "",
        )

        view.update(state)

        view.binding.requestSupport.performClick()

        verify {
            interactor.handleRequestSiteSupportPressed()
        }
    }
}
