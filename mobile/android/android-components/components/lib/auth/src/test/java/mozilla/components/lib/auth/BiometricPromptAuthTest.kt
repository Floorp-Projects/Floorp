/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.auth

import androidx.biometric.BiometricPrompt
import androidx.fragment.app.Fragment
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.createAddedTestFragment
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class BiometricPromptAuthTest {

    private lateinit var biometricPromptAuth: BiometricPromptAuth
    private lateinit var fragment: Fragment

    @Before
    fun setup() {
        fragment = createAddedTestFragment { Fragment() }
        biometricPromptAuth = BiometricPromptAuth(
            testContext,
            fragment,
            object : AuthenticationDelegate {
                override fun onAuthFailure() {
                }

                override fun onAuthSuccess() {
                }

                override fun onAuthError(errorText: String) {
                }
            },
        )
    }

    @Test
    fun `prompt is created and destroyed on start and stop`() {
        assertNull(biometricPromptAuth.biometricPrompt)

        biometricPromptAuth.start()

        assertNotNull(biometricPromptAuth.biometricPrompt)

        biometricPromptAuth.stop()

        assertNull(biometricPromptAuth.biometricPrompt)
    }

    @Test
    fun `requestAuthentication invokes biometric prompt`() {
        val prompt: BiometricPrompt = mock()

        biometricPromptAuth.biometricPrompt = prompt

        biometricPromptAuth.requestAuthentication("title", "subtitle")

        verify(prompt).authenticate(any())
    }

    @Test
    fun `promptCallback fires feature callbacks`() {
        val authenticationDelegate: AuthenticationDelegate = mock()
        val feature = BiometricPromptAuth(testContext, fragment, authenticationDelegate)
        val callback = feature.PromptCallback()
        val prompt = BiometricPrompt(fragment, callback)

        feature.biometricPrompt = prompt

        callback.onAuthenticationError(BiometricPrompt.ERROR_CANCELED, "")

        verify(authenticationDelegate).onAuthError("")

        callback.onAuthenticationFailed()

        verify(authenticationDelegate).onAuthFailure()

        callback.onAuthenticationSucceeded(mock())

        verify(authenticationDelegate).onAuthSuccess()
    }
}
