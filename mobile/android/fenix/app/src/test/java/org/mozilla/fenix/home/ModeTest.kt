/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.content.Context
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.service.fxa.manager.FxaAccountManager
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.onboarding.FenixOnboarding
import org.mozilla.fenix.onboarding.OnboardingState

class ModeTest {

    private lateinit var context: Context
    private lateinit var accountManager: FxaAccountManager
    private lateinit var onboarding: FenixOnboarding
    private lateinit var browsingModeManager: BrowsingModeManager
    private lateinit var currentMode: CurrentMode
    private lateinit var dispatchModeChanges: (mode: Mode) -> Unit
    private var dispatchModeChangesResult: Mode? = null

    @Before
    fun setup() {
        context = mockk(relaxed = true)
        accountManager = mockk(relaxed = true)
        onboarding = mockk(relaxed = true)
        browsingModeManager = mockk(relaxed = true)

        dispatchModeChangesResult = null
        dispatchModeChanges = {
            dispatchModeChangesResult = it
        }

        every { context.components.backgroundServices.accountManager } returns accountManager

        currentMode = CurrentMode(
            context,
            onboarding,
            browsingModeManager,
            dispatchModeChanges,
        )
    }

    @Test
    fun `get current mode after onboarding`() {
        every { onboarding.userHasBeenOnboarded() } returns true
        every { browsingModeManager.mode } returns BrowsingMode.Normal

        assertEquals(Mode.Normal, currentMode.getCurrentMode())
    }

    @Test
    fun `get current private mode after onboarding`() {
        every { onboarding.userHasBeenOnboarded() } returns true
        every { browsingModeManager.mode } returns BrowsingMode.Private

        assertEquals(Mode.Private, currentMode.getCurrentMode())
    }

    @Test
    fun `get current browsing mode when userHasBeenOnboarded is false and juno onboarding is enabled`() {
        every { onboarding.userHasBeenOnboarded() } returns false
        every { context.components.settings.junoOnboardingEnabled } returns true
        every { browsingModeManager.mode } returns BrowsingMode.Normal

        assertEquals(Mode.Normal, currentMode.getCurrentMode())
    }

    @Test
    fun `get current onboarding mode when signed in`() {
        every { onboarding.userHasBeenOnboarded() } returns false
        every { accountManager.authenticatedAccount() } returns mockk()

        assertEquals(Mode.Onboarding(OnboardingState.SignedIn, onboarding.config), currentMode.getCurrentMode())
    }

    @Test
    fun `get current onboarding mode when signed out`() {
        every { onboarding.userHasBeenOnboarded() } returns false
        every { accountManager.authenticatedAccount() } returns null

        assertEquals(Mode.Onboarding(OnboardingState.SignedOutNoAutoSignIn, onboarding.config), currentMode.getCurrentMode())
    }

    @Test
    fun `emit mode change`() {
        every { onboarding.userHasBeenOnboarded() } returns true
        every { browsingModeManager.mode } returns BrowsingMode.Normal

        currentMode.emitModeChanges()

        assertEquals(Mode.Normal, dispatchModeChangesResult)
    }

    @Test
    fun `account observer calls emitModeChanges`() {
        val spy = spyk(currentMode)

        spy.onAuthenticated(mockk(), mockk())
        verify { spy.emitModeChanges() }

        spy.onAuthenticationProblems()
        verify { spy.emitModeChanges() }

        spy.onLoggedOut()
        verify { spy.emitModeChanges() }

        spy.onProfileUpdated(mockk())
        verify { spy.emitModeChanges() }
    }
}
