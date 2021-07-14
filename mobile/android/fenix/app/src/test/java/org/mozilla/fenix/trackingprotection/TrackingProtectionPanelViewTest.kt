/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.core.view.isVisible
import io.mockk.mockk
import io.mockk.verify
import kotlinx.android.synthetic.main.component_tracking_protection_panel.*
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
        assertFalse(view.details_mode.isVisible)
        assertTrue(view.normal_mode.isVisible)
        assertTrue(view.protection_settings.isVisible)
        assertFalse(view.not_blocking_header.isVisible)
        assertFalse(view.blocking_header.isVisible)
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
        assertTrue(view.details_mode.isVisible)
        assertFalse(view.normal_mode.isVisible)
        assertEquals(
            testContext.getString(R.string.etp_tracking_content_title),
            view.category_title.text
        )
        assertEquals(
            testContext.getString(R.string.etp_tracking_content_description),
            view.category_description.text
        )
        assertEquals(
            testContext.getString(R.string.enhanced_tracking_protection_allowed),
            view.details_blocking_header.text
        )
    }

    @Test
    fun testProtectionSettings() {
        view.protection_settings.performClick()
        verify { interactor.selectTrackingProtectionSettings() }
    }

    @Test
    fun testExistDetailModed() {
        view.details_back.performClick()
        verify { interactor.onExitDetailMode() }
    }

    @Test
    fun testDetailsBack() {
        view.navigate_back.performClick()
        verify { interactor.onBackPressed() }
    }

    @Test
    fun testSocialMediaTrackerClick() {
        view.social_media_trackers.performClick()
        verify { interactor.openDetails(SOCIAL_MEDIA_TRACKERS, categoryBlocked = true) }

        view.social_media_trackers_loaded.performClick()
        verify { interactor.openDetails(SOCIAL_MEDIA_TRACKERS, categoryBlocked = false) }
    }

    @Test
    fun testCrossSiteTrackerClick() {
        view.cross_site_tracking.performClick()
        verify { interactor.openDetails(CROSS_SITE_TRACKING_COOKIES, categoryBlocked = true) }

        view.cross_site_tracking_loaded.performClick()
        verify { interactor.openDetails(CROSS_SITE_TRACKING_COOKIES, categoryBlocked = false) }
    }
}
