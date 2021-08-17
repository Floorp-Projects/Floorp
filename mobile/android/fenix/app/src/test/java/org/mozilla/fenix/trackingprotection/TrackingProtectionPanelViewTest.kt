/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.core.view.isVisible
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.CROSS_SITE_TRACKING_COOKIES
import org.mozilla.fenix.trackingprotection.TrackingProtectionCategory.SOCIAL_MEDIA_TRACKERS

@RunWith(FenixRobolectricTestRunner::class)
class TrackingProtectionPanelViewTest {

    private lateinit var container: ViewGroup
    private lateinit var interactor: TrackingProtectionPanelInteractor
    private lateinit var view: TrackingProtectionPanelView
    private val baseState = TrackingProtectionState(
        tab = null,
        url = "",
        isTrackingProtectionEnabled = false,
        listTrackers = emptyList(),
        mode = TrackingProtectionState.Mode.Normal,
        lastAccessedCategory = ""
    )

    @Before
    fun setup() {
        container = FrameLayout(testContext)
        interactor = mockk(relaxUnitFun = true)
        view = TrackingProtectionPanelView(container, interactor)
    }

    @Test
    fun testNormalModeUi() {
        view.update(baseState.copy(mode = TrackingProtectionState.Mode.Normal))
        assertFalse(view.binding.detailsMode.isVisible)
        assertTrue(view.binding.normalMode.isVisible)
        assertTrue(view.binding.protectionSettings.isVisible)
        assertFalse(view.binding.notBlockingHeader.isVisible)
        assertFalse(view.binding.blockingHeader.isVisible)
    }

    @Test
    fun testPrivateModeUi() {
        view.update(
            baseState.copy(
                mode = TrackingProtectionState.Mode.Details(
                    selectedCategory = TrackingProtectionCategory.TRACKING_CONTENT,
                    categoryBlocked = false
                )
            )
        )
        assertTrue(view.binding.detailsMode.isVisible)
        assertFalse(view.binding.normalMode.isVisible)
        assertEquals(
            testContext.getString(R.string.etp_tracking_content_title),
            view.binding.categoryTitle.text
        )
        assertEquals(
            testContext.getString(R.string.etp_tracking_content_description),
            view.binding.categoryDescription.text
        )
        assertEquals(
            testContext.getString(R.string.enhanced_tracking_protection_allowed),
            view.binding.detailsBlockingHeader.text
        )
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
        view.binding.socialMediaTrackers.performClick()
        verify { interactor.openDetails(SOCIAL_MEDIA_TRACKERS, categoryBlocked = true) }

        view.binding.socialMediaTrackersLoaded.performClick()
        verify { interactor.openDetails(SOCIAL_MEDIA_TRACKERS, categoryBlocked = false) }
    }

    @Test
    fun testCrossSiteTrackerClick() {
        view.binding.crossSiteTracking.performClick()
        verify { interactor.openDetails(CROSS_SITE_TRACKING_COOKIES, categoryBlocked = true) }

        view.binding.crossSiteTrackingLoaded.performClick()
        verify { interactor.openDetails(CROSS_SITE_TRACKING_COOKIES, categoryBlocked = false) }
    }
}
