/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import android.content.Context
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.core.view.isVisible
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.verify
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.TrackingProtection
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.CROSS_SITE_TRACKING_COOKIES
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.SOCIAL_MEDIA_TRACKERS

@RunWith(FenixRobolectricTestRunner::class)
class TrackingProtectionPanelViewTest {

    private lateinit var container: ViewGroup
    private lateinit var interactor: TrackingProtectionPanelInteractor
    private lateinit var view: TrackingProtectionPanelView
    private val baseState = ProtectionsState(
        tab = null,
        url = "",
        isTrackingProtectionEnabled = false,
        cookieBannerUIMode = CookieBannerUIMode.DISABLE,
        listTrackers = emptyList(),
        mode = ProtectionsState.Mode.Normal,
        lastAccessedCategory = "",
    )

    @get:Rule
    val gleanRule = GleanTestRule(testContext)

    @Before
    fun setup() {
        container = FrameLayout(testContext)
        interactor = mockk(relaxUnitFun = true)
        view = TrackingProtectionPanelView(container, interactor)
    }

    @Test
    fun testNormalModeUi() {
        mockkStatic("org.mozilla.fenix.ext.ContextKt") {
            every { any<Context>().settings() } returns mockk(relaxed = true)

            view.update(baseState.copy(mode = ProtectionsState.Mode.Normal))
            assertFalse(view.binding.detailsMode.isVisible)
            assertTrue(view.binding.normalMode.isVisible)
            assertTrue(view.binding.protectionSettings.isVisible)
            assertFalse(view.binding.notBlockingHeader.isVisible)
            assertFalse(view.binding.blockingHeader.isVisible)
        }
    }

    @Test
    fun testNormalModeUiCookiesWithTotalCookieProtectionEnabled() {
        mockkStatic("org.mozilla.fenix.ext.ContextKt") {
            every { any<Context>().settings() } returns mockk {
                every { enabledTotalCookieProtection } returns true
            }
            val expectedTitle = testContext.getString(R.string.etp_cookies_title_2)

            view.update(baseState.copy(mode = ProtectionsState.Mode.Normal))

            assertEquals(expectedTitle, view.binding.crossSiteTracking.text)
            assertEquals(expectedTitle, view.binding.crossSiteTrackingLoaded.text)
        }
    }

    @Test
    fun testNormalModeUiCookiesWithTotalCookieProtectionDisabled() {
        mockkStatic("org.mozilla.fenix.ext.ContextKt") {
            every { any<Context>().settings() } returns mockk {
                every { enabledTotalCookieProtection } returns false
            }
            val expectedTitle = testContext.getString(R.string.etp_cookies_title)

            view.update(baseState.copy(mode = ProtectionsState.Mode.Normal))

            assertEquals(expectedTitle, view.binding.crossSiteTracking.text)
            assertEquals(expectedTitle, view.binding.crossSiteTrackingLoaded.text)
        }
    }

    @Test
    fun testPrivateModeUi() {
        view.update(
            baseState.copy(
                mode = ProtectionsState.Mode.Details(
                    selectedCategory = TrackingProtectionCategory.TRACKING_CONTENT,
                    categoryBlocked = false,
                ),
            ),
        )
        assertTrue(view.binding.detailsMode.isVisible)
        assertFalse(view.binding.normalMode.isVisible)
        assertEquals(
            testContext.getString(R.string.etp_tracking_content_title),
            view.binding.categoryTitle.text,
        )
        assertEquals(
            testContext.getString(R.string.etp_tracking_content_description),
            view.binding.categoryDescription.text,
        )
        assertEquals(
            testContext.getString(R.string.enhanced_tracking_protection_allowed),
            view.binding.detailsBlockingHeader.text,
        )
    }

    @Test
    fun testPrivateModeUiCookiesWithTotalCookieProtectionEnabled() {
        mockkStatic("org.mozilla.fenix.ext.ContextKt") {
            every { any<Context>().settings() } returns mockk {
                every { enabledTotalCookieProtection } returns true
            }
            val expectedTitle = testContext.getString(R.string.etp_cookies_title_2)
            val expectedDescription = testContext.getString(R.string.etp_cookies_description_2)

            view.update(
                baseState.copy(
                    mode = ProtectionsState.Mode.Details(
                        selectedCategory = CROSS_SITE_TRACKING_COOKIES,
                        categoryBlocked = false,
                    ),
                ),
            )

            assertEquals(expectedTitle, view.binding.categoryTitle.text)
            assertEquals(expectedDescription, view.binding.categoryDescription.text)
        }
    }

    @Test
    fun testPrivateModeUiCookiesWithTotalCookieProtectionDisabled() {
        mockkStatic("org.mozilla.fenix.ext.ContextKt") {
            every { any<Context>().settings() } returns mockk {
                every { enabledTotalCookieProtection } returns false
            }
            val expectedTitle = testContext.getString(R.string.etp_cookies_title)
            val expectedDescription = testContext.getString(R.string.etp_cookies_description)

            view.update(
                baseState.copy(
                    mode = ProtectionsState.Mode.Details(
                        selectedCategory = CROSS_SITE_TRACKING_COOKIES,
                        categoryBlocked = false,
                    ),
                ),
            )

            assertEquals(expectedTitle, view.binding.categoryTitle.text)
            assertEquals(expectedDescription, view.binding.categoryDescription.text)
        }
    }

    @Test
    fun testProtectionSettings() {
        view.binding.protectionSettings.performClick()
        verify { interactor.selectTrackingProtectionSettings() }
    }

    @Test
    fun testExistDetailModed() {
        view.binding.detailsBack.performClick()
        verify { interactor.onExitDetailMode() }
    }

    @Test
    fun testDetailsBack() {
        view.binding.navigateBack.performClick()
        verify { interactor.onBackPressed() }
    }

    @Test
    fun testSocialMediaTrackerClick() {
        every { testContext.components.analytics } returns mockk(relaxed = true)
        view.binding.socialMediaTrackers.performClick()
        verify { interactor.openDetails(SOCIAL_MEDIA_TRACKERS, categoryBlocked = true) }

        view.binding.socialMediaTrackersLoaded.performClick()
        verify { interactor.openDetails(SOCIAL_MEDIA_TRACKERS, categoryBlocked = false) }
    }

    @Test
    fun testCrossSiteTrackerClick() {
        every { testContext.components.analytics } returns mockk(relaxed = true)
        assertNull(TrackingProtection.etpTrackerList.testGetValue())

        view.binding.crossSiteTracking.performClick()

        assertNotNull(TrackingProtection.etpTrackerList.testGetValue())
        verify { interactor.openDetails(CROSS_SITE_TRACKING_COOKIES, categoryBlocked = true) }

        view.binding.crossSiteTrackingLoaded.performClick()
        verify { interactor.openDetails(CROSS_SITE_TRACKING_COOKIES, categoryBlocked = false) }
    }
}
