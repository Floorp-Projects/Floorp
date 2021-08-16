/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import android.content.Context
import androidx.fragment.app.Fragment
import androidx.navigation.NavController
import androidx.navigation.NavDirections
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.slot
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.feature.session.TrackingProtectionUseCases
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class TrackingProtectionPanelInteractorTest {

    private lateinit var context: Context

    @MockK(relaxed = true)
    private lateinit var navController: NavController

    @MockK(relaxed = true)
    private lateinit var fragment: Fragment

    @MockK(relaxed = true)
    private lateinit var sitePermissions: SitePermissions

    @MockK(relaxed = true)
    private lateinit var store: TrackingProtectionStore

    private lateinit var interactor: TrackingProtectionPanelInteractor

    private lateinit var tab: TabSessionState

    private var openSettings = false
    private var gravity = 54

    @Before
    fun setup() {
        MockKAnnotations.init(this)

        context = spyk(testContext)
        tab = createTab("https://mozilla.org")

        interactor = TrackingProtectionPanelInteractor(
            context = context,
            fragment = fragment,
            store = store,
            navController = { navController },
            openTrackingProtectionSettings = { openSettings = true },
            sitePermissions = sitePermissions,
            gravity = gravity,
            getCurrentTab = { tab }
        )

        val trackingProtectionUseCases: TrackingProtectionUseCases = mockk(relaxed = true)

        every { fragment.context } returns context
        every { context.components.useCases.trackingProtectionUseCases } returns trackingProtectionUseCases

        val onComplete = slot<(Boolean) -> Unit>()
        every {
            trackingProtectionUseCases.containsException.invoke(
                any(),
                capture(onComplete)
            )
        }.answers { onComplete.captured.invoke(true) }
    }

    @Test
    fun `WHEN openDetails is called THEN store should dispatch EnterDetailsMode action with the right category`() {
        interactor.openDetails(TrackingProtectionCategory.FINGERPRINTERS, true)

        verify {
            store.dispatch(
                TrackingProtectionAction.EnterDetailsMode(
                    TrackingProtectionCategory.FINGERPRINTERS,
                    true
                )
            )
        }

        interactor.openDetails(TrackingProtectionCategory.REDIRECT_TRACKERS, true)

        verify {
            store.dispatch(
                TrackingProtectionAction.EnterDetailsMode(
                    TrackingProtectionCategory.REDIRECT_TRACKERS,
                    true
                )
            )
        }
    }

    @Test
    fun `WHEN selectTrackingProtectionSettings is called THEN openTrackingProtectionSettings should be invoked`() {
        interactor.selectTrackingProtectionSettings()

        assertEquals(true, openSettings)
    }

    @Test
    fun `WHEN onBackPressed is called THEN call popBackStack and navigate`() {
        interactor.onBackPressed()

        verify {
            navController.popBackStack()

            navController.navigate(any<NavDirections>())
        }
    }

    @Test
    fun `WHEN onExitDetailMode is called THEN store should dispatch ExitDetailsMode action`() {
        interactor.onExitDetailMode()

        verify { store.dispatch(TrackingProtectionAction.ExitDetailsMode) }
    }
}
