/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import android.content.Context
import androidx.fragment.app.Fragment
import androidx.navigation.NavController
import androidx.navigation.NavDirections
import io.mockk.MockKAnnotations
import io.mockk.coVerify
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.slot
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.concept.engine.cookiehandling.CookieBannersStorage
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.feature.session.TrackingProtectionUseCases
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
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
    private lateinit var store: ProtectionsStore

    private lateinit var interactor: TrackingProtectionPanelInteractor

    private lateinit var tab: TabSessionState

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    private var learnMoreClicked = false
    private var openSettings = false
    private var gravity = 54

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        learnMoreClicked = false

        context = spyk(testContext)
        tab = createTab("https://mozilla.org")
        val cookieBannersStorage: CookieBannersStorage = mockk(relaxed = true)

        interactor = TrackingProtectionPanelInteractor(
            context = context,
            fragment = fragment,
            store = store,
            ioScope = scope,
            cookieBannersStorage = cookieBannersStorage,
            navController = { navController },
            openTrackingProtectionSettings = { openSettings = true },
            openLearnMoreLink = { learnMoreClicked = true },
            sitePermissions = sitePermissions,
            gravity = gravity,
            getCurrentTab = { tab },
        )

        val trackingProtectionUseCases: TrackingProtectionUseCases = mockk(relaxed = true)

        every { fragment.context } returns context
        every { context.components.useCases.trackingProtectionUseCases } returns trackingProtectionUseCases

        val onComplete = slot<(Boolean) -> Unit>()
        every {
            trackingProtectionUseCases.containsException.invoke(
                any(),
                capture(onComplete),
            )
        }.answers { onComplete.captured.invoke(true) }
    }

    @Test
    fun `WHEN openDetails is called THEN store should dispatch EnterDetailsMode action with the right category`() {
        interactor.openDetails(TrackingProtectionCategory.FINGERPRINTERS, true)

        verify {
            store.dispatch(
                ProtectionsAction.EnterDetailsMode(
                    TrackingProtectionCategory.FINGERPRINTERS,
                    true,
                ),
            )
        }

        interactor.openDetails(TrackingProtectionCategory.REDIRECT_TRACKERS, true)

        verify {
            store.dispatch(
                ProtectionsAction.EnterDetailsMode(
                    TrackingProtectionCategory.REDIRECT_TRACKERS,
                    true,
                ),
            )
        }
    }

    @Test
    fun `WHEN selectTrackingProtectionSettings is called THEN openTrackingProtectionSettings should be invoked`() {
        interactor.selectTrackingProtectionSettings()

        assertEquals(true, openSettings)
    }

    @Test
    fun `WHEN on the learn more link is clicked THEN onLearnMoreClicked should be invoked`() {
        interactor.onLearnMoreClicked()

        assertEquals(true, learnMoreClicked)
    }

    @Test
    fun `WHEN onBackPressed is called THEN call popBackStack and navigate`() = runTestOnMain {
        interactor.onBackPressed()

        coVerify {
            navController.popBackStack()

            navController.navigate(any<NavDirections>())
        }
    }

    @Test
    fun `WHEN onExitDetailMode is called THEN store should dispatch ExitDetailsMode action`() {
        interactor.onExitDetailMode()

        verify { store.dispatch(ProtectionsAction.ExitDetailsMode) }
    }
}
