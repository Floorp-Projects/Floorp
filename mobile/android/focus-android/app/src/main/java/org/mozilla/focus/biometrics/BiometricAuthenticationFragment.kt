/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.biometrics

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.platform.ComposeView
import mozilla.components.lib.auth.AuthenticationDelegate
import mozilla.components.lib.auth.BiometricPromptAuth
import mozilla.components.lib.auth.canUseBiometricFeature
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.mozilla.focus.R
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.fragment.BaseFragment
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.ui.theme.FocusTheme

class BiometricAuthenticationFragment : BaseFragment(), AuthenticationDelegate {
    @VisibleForTesting
    internal val biometricPromptAuth = ViewBoundFeatureWrapper<BiometricPromptAuth>()
    @VisibleForTesting
    internal val biometricErrorText = mutableStateOf("")

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        return ComposeView(requireContext()).apply {
            setBiometricPrompt(this)
            setContent {
                FocusTheme {
                    val biometricErrorText by biometricErrorText
                    BiometricPromptContent(biometricErrorText) {
                        showBiometricPrompt(
                            biometricPromptAuth.get()
                        )
                    }
                }
            }
        }
    }

    override fun onAuthError(errorText: String) {
        biometricErrorText.value = errorText
    }

    override fun onAuthFailure() {
        // onAuthFailure
    }

    override fun onAuthSuccess() {
        onAuthenticated()
    }

    @VisibleForTesting
    internal fun showBiometricPrompt(biometricPromptAuth: BiometricPromptAuth?) {
        if (context?.canUseBiometricFeature() == true) {
            biometricPromptAuth?.requestAuthentication(
                title = getString(R.string.biometric_prompt_title),
                subtitle = getString(
                    R.string.biometric_prompt_subtitle
                )
            )
        }
    }

    private fun setBiometricPrompt(view: View) {
        biometricPromptAuth.set(
            feature = BiometricPromptAuth(
                context = requireContext(),
                fragment = this,
                authenticationDelegate = this
            ),
            owner = this,
            view = view
        )
    }

    @VisibleForTesting
    internal fun onAuthenticated() {
        val tabId = requireComponents.store.state.selectedTabId
        requireComponents.appStore.dispatch(AppAction.Unlock(tabId))
        dismiss()
    }

    @VisibleForTesting
    internal fun dismiss() {
        requireActivity().supportFragmentManager.beginTransaction().remove(this).commitAllowingStateLoss()
    }

    companion object {
        const val FRAGMENT_TAG = "biometric-authentication-fragment"
    }
}
