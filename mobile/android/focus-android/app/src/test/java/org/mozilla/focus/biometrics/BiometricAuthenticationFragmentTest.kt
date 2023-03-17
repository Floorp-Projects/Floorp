/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.biometrics

import android.content.Context
import android.os.Build
import androidx.fragment.app.FragmentActivity
import androidx.fragment.app.FragmentManager
import androidx.fragment.app.FragmentTransaction
import mozilla.components.lib.auth.AuthenticationDelegate
import mozilla.components.lib.auth.BiometricPromptAuth
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.annotation.Config

class BiometricAuthenticationFragmentTest {
    private lateinit var biometricPromptAuth: BiometricPromptAuth
    private lateinit var fragment: BiometricAuthenticationFragment
    private val activity: FragmentActivity = mock()
    private val testContext: Context = mock()

    private val fragmentManger: FragmentManager = mock()
    private val fragmentTransaction: FragmentTransaction = mock()

    @Before
    fun setup() {
        fragment = spy(BiometricAuthenticationFragment())
        doReturn(testContext).`when`(fragment).context
        doReturn(activity).`when`(fragment).requireActivity()
        doReturn(fragmentManger).`when`(activity).supportFragmentManager
        doReturn(fragmentTransaction).`when`(fragmentManger).beginTransaction()
        biometricPromptAuth = spy(
            BiometricPromptAuth(
                testContext,
                fragment,
                object : AuthenticationDelegate {
                    override fun onAuthError(errorText: String) {
                    }
                    override fun onAuthFailure() {
                    }
                    override fun onAuthSuccess() {
                    }
                },
            ),
        )
    }

    @Config(sdk = [Build.VERSION_CODES.LOLLIPOP])
    @Test
    fun `GIVEN biometric authentication fragment WHEN show biometric prompt is called and can use feature returns false THEN request authentication is not called`() {
        fragment.showBiometricPrompt(biometricPromptAuth, "title", "subtitle")

        verify(biometricPromptAuth, never()).requestAuthentication("title", "subtitle")
    }

    @Test
    fun `GIVEN biometric authentication fragment WHEN on Auth Error is  called THEN biometricErrorText should be updated`() {
        fragment.onAuthError("Fingerprint operation canceled by user.")

        assertEquals(fragment.biometricErrorText.value, "Fingerprint operation canceled by user.")
    }
}
