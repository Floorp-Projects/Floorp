/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.LayoutInflater
import android.view.View
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.unmockkObject
import io.mockk.verify
import kotlinx.android.synthetic.main.onboarding_automatic_signin.view.*
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.service.fxa.manager.SignInWithShareableAccountResult
import mozilla.components.service.fxa.sharing.ShareableAccount
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.BackgroundServices
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@ExperimentalCoroutinesApi
@RunWith(FenixRobolectricTestRunner::class)
class OnboardingAutomaticSignInViewHolderTest {

    private lateinit var view: View
    private lateinit var backgroundServices: BackgroundServices
    private lateinit var snackbar: FenixSnackbar

    @Before
    fun setup() {
        view = LayoutInflater.from(testContext)
            .inflate(OnboardingAutomaticSignInViewHolder.LAYOUT_ID, null)
        snackbar = mockk(relaxed = true)
        mockkObject(FenixSnackbar.Companion)

        backgroundServices = testContext.components.backgroundServices
        every { FenixSnackbar.make(any(), any(), any(), any()) } returns snackbar
    }

    @After
    fun teardown() {
        unmockkObject(FenixSnackbar.Companion)
    }

    @Test
    fun `bind updates header text`() {
        val holder = OnboardingAutomaticSignInViewHolder(view)
        holder.bind(mockk {
            every { email } returns "email@example.com"
        })
        assertEquals(
            "You are signed in as email@example.com on another Firefox browser on this phone. Would you like to sign in with this account?",
            view.header_text.text
        )
        assertTrue(view.fxa_sign_in_button.isEnabled)
    }

    @Test
    fun `sign in on click`() = runBlocking {
        val account = mockk<ShareableAccount> {
            every { email } returns "email@example.com"
        }
        every {
            backgroundServices.accountManager.signInWithShareableAccountAsync(account)
        } returns CompletableDeferred(SignInWithShareableAccountResult.Success)

        val holder = OnboardingAutomaticSignInViewHolder(view, scope = this)
        holder.bind(account)
        holder.onClick(view.fxa_sign_in_button)

        assertEquals("Signing in…", view.fxa_sign_in_button.text)
        assertFalse(view.fxa_sign_in_button.isEnabled)
    }

    @Test
    fun `show error if sign in fails`() = runBlockingTest {
        val account = mockk<ShareableAccount> {
            every { email } returns "email@example.com"
        }
        every {
            backgroundServices.accountManager.signInWithShareableAccountAsync(account)
        } returns CompletableDeferred(SignInWithShareableAccountResult.Failure)

        val holder = OnboardingAutomaticSignInViewHolder(view, scope = this)
        holder.bind(account)
        holder.onClick(view.fxa_sign_in_button)

        assertEquals("Yes, sign me in", view.fxa_sign_in_button.text)
        assertTrue(view.fxa_sign_in_button.isEnabled)
        verify { snackbar.setText("Failed to sign-in") }
    }
}
