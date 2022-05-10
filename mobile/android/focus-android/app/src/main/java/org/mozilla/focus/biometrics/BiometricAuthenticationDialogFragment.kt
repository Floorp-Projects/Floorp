/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.biometrics

import android.content.DialogInterface
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.RequiresApi
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.core.content.ContextCompat
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.LifecycleObserver
import org.mozilla.focus.R
import org.mozilla.focus.databinding.BiometricPromptDialogContentBinding
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.topsites.TopSitesIntegration
import org.mozilla.focus.utils.Features.DELETE_TOP_SITES_WHEN_NEW_SESSION_BUTTON_CLICKED

@RequiresApi(api = Build.VERSION_CODES.M)
@Suppress("TooManyFunctions")
class BiometricAuthenticationDialogFragment : AppCompatDialogFragment(), LifecycleObserver {
    private var handler: BiometricAuthenticationHandler? = null
    private var _binding: BiometricPromptDialogContentBinding? = null
    private val binding get() = _binding!!
    private val topSitesIntegration by lazy { TopSitesIntegration(requireComponents.topSitesStorage) }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(DialogFragment.STYLE_NORMAL, R.style.DialogStyle)
        lifecycle.addObserver(topSitesIntegration)
    }

    override fun onCancel(dialog: DialogInterface) {
        super.onCancel(dialog)
        val startMain = Intent(Intent.ACTION_MAIN)
        startMain.addCategory(Intent.CATEGORY_HOME)
        startMain.flags = Intent.FLAG_ACTIVITY_NEW_TASK
        startActivity(startMain)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = BiometricPromptDialogContentBinding.inflate(
            inflater,
            container,
            false
        )

        val biometricDialog = dialog
        this.isCancelable = true
        biometricDialog?.setCanceledOnTouchOutside(false)
        biometricDialog?.setTitle(
            getString(R.string.biometric_auth_title, getString(R.string.app_name))
        )

        binding.description.setText(R.string.biometric_auth_description)
        binding.newSessionButton.setText(R.string.biometric_auth_new_session)
        binding.newSessionButton.setOnClickListener {
            triggerNewSession()
        }

        binding.fingerprintIcon.setImageResource(R.drawable.ic_fingerprint)
        return binding.root
    }

    override fun onResume() {
        super.onResume()

        resetErrorText()
        if (!Biometrics.hasFingerprintHardware(requireContext())) {
            triggerNewSession()
        } else {
            handler = BiometricAuthenticationHandler(requireContext(), this)
            handler?.startAuthentication()
        }
    }

    override fun onPause() {
        super.onPause()

        handler?.stopListening()
        handler = null
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private fun triggerNewSession() {
        requireComponents.tabsUseCases.removePrivateTabs()
        requireComponents.appStore.dispatch(AppAction.ShowHomeScreen)
        if (DELETE_TOP_SITES_WHEN_NEW_SESSION_BUTTON_CLICKED) {
            topSitesIntegration.deleteAllTopSites()
        }
        dismiss()
    }

    fun displayError(errorText: String) {
        // Display the error text
        binding.biometricErrorText.text = errorText
        binding.biometricErrorText.setTextColor(
            ContextCompat.getColor(
                requireContext(),
                R.color.error
            )
        )
    }

    fun onAuthenticated() {
        val tabId = requireComponents.store.state.selectedTabId
        requireComponents.appStore.dispatch(AppAction.Unlock(tabId))
        dismiss()
    }

    fun onFailure() {
        displayError(getString(R.string.biometric_auth_not_recognized_error))
    }

    private fun resetErrorText() {
        binding.biometricErrorText.text = ""
    }

    companion object {
        const val FRAGMENT_TAG = "biometric-dialog-fragment"
    }
}
