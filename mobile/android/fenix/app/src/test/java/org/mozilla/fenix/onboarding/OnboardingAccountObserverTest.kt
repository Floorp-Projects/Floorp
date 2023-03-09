/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import android.content.Context
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.service.fxa.manager.FxaAccountManager
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.ext.components

class OnboardingAccountObserverTest {

    private lateinit var context: Context
    private lateinit var accountManager: FxaAccountManager
    private lateinit var observer: OnboardingAccountObserver
    private lateinit var dispatchChanges: (mode: OnboardingState) -> Unit

    private var dispatchChangesResult: OnboardingState? = null

    @Before
    fun setup() {
        context = mockk(relaxed = true)
        accountManager = mockk(relaxed = true)

        dispatchChangesResult = null
        dispatchChanges = {
            dispatchChangesResult = it
        }

        every { context.components.backgroundServices.accountManager } returns accountManager

        observer = spyk(
            OnboardingAccountObserver(
                context = context,
                dispatchChanges = dispatchChanges,
            ),
        )
    }

    @Test
    fun `WHEN account is authenticated THEN return signed in state`() {
        every { accountManager.authenticatedAccount() } returns mockk()

        assertEquals(OnboardingState.SignedIn, observer.getOnboardingState())
    }

    @Test
    fun `WHEN account is not authenticated THEN return signed out state`() {
        every { accountManager.authenticatedAccount() } returns null

        assertEquals(OnboardingState.SignedOutNoAutoSignIn, observer.getOnboardingState())
    }

    @Test
    fun `WHEN account observer receives a state change THEN emit state changes`() {
        observer.onAuthenticated(mockk(), mockk())
        observer.onAuthenticationProblems()
        observer.onLoggedOut()
        observer.onProfileUpdated(mockk())

        verify(exactly = 4) { observer.emitChanges() }
    }
}
