/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.LayoutInflater
import io.mockk.every
import io.mockk.coEvery
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.verify
import io.mockk.unmockkObject
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.service.fxa.manager.MigrationResult
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
import org.mozilla.fenix.databinding.OnboardingAutomaticSigninBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@ExperimentalCoroutinesApi
@RunWith(FenixRobolectricTestRunner::class)
class OnboardingAutomaticSignInViewHolderTest {

    private lateinit var backgroundServices: BackgroundServices
    private lateinit var snackbar: FenixSnackbar
    private lateinit var binding: OnboardingAutomaticSigninBinding

    @Before
    fun setup() {
        binding = OnboardingAutomaticSigninBinding.inflate(LayoutInflater.from(testContext))
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
        val holder = OnboardingAutomaticSignInViewHolder(binding.root)
        holder.bind(
            mockk {
                every { email } returns "email@example.com"
            }
        )
        assertEquals(
            "You are signed in as email@example.com on another Firefox browser on this device. Would you like to sign in with this account?",
            binding.headerText.text
        )
        assertTrue(binding.fxaSignInButton.isEnabled)
    }

    @Test
    fun `sign in on click - MigrationResult Success`() = runBlocking {
        val account = mockk<ShareableAccount> {
            every { email } returns "email@example.com"
        }
        coEvery {
            backgroundServices.accountManager.migrateFromAccount(account)
        } returns MigrationResult.Success

        val holder = OnboardingAutomaticSignInViewHolder(binding.root, scope = this)
        holder.bind(account)
        holder.onClick(binding.fxaSignInButton)

        assertEquals("Signing in…", binding.fxaSignInButton.text)
        assertFalse(binding.fxaSignInButton.isEnabled)
    }

    @Test
    fun `sign in on click - MigrationResult WillRetry treated the same as Success`() = runBlocking {
        val account = mockk<ShareableAccount> {
            every { email } returns "email@example.com"
        }
        coEvery {
            backgroundServices.accountManager.migrateFromAccount(account)
        } returns MigrationResult.WillRetry

        val holder = OnboardingAutomaticSignInViewHolder(binding.root, scope = this)
        holder.bind(account)
        holder.onClick(binding.fxaSignInButton)

        assertEquals("Signing in…", binding.fxaSignInButton.text)
        assertFalse(binding.fxaSignInButton.isEnabled)
    }

    @Test
    fun `show error if sign in fails`() = runBlockingTest {
        val account = mockk<ShareableAccount> {
            every { email } returns "email@example.com"
        }
        coEvery {
            backgroundServices.accountManager.migrateFromAccount(account)
        } returns MigrationResult.Failure

        val holder = OnboardingAutomaticSignInViewHolder(binding.root, scope = this)
        holder.bind(account)
        holder.onClick(binding.fxaSignInButton)

        assertEquals("Yes, sign me in", binding.fxaSignInButton.text)
        assertTrue(binding.fxaSignInButton.isEnabled)
        verify { snackbar.setText("Failed to sign-in") }
    }
}
