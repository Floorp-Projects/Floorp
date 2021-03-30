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
import kotlinx.android.synthetic.main.biometric_prompt_dialog_content.*
import kotlinx.android.synthetic.main.biometric_prompt_dialog_content.view.*
import org.mozilla.focus.R
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.state.AppAction

@RequiresApi(api = Build.VERSION_CODES.M)
class BiometricAuthenticationDialogFragment : AppCompatDialogFragment(), LifecycleObserver {
    private var handler: BiometricAuthenticationHandler? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(DialogFragment.STYLE_NORMAL, android.R.style.Theme_Material_Light_Dialog)
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
    ): View? {
        val v = inflater.inflate(
            R.layout.biometric_prompt_dialog_content,
            container,
            false
        )

        val biometricDialog = dialog
        this.isCancelable = true
        biometricDialog?.setCanceledOnTouchOutside(false)
        biometricDialog?.setTitle(
            getString(R.string.biometric_auth_title, getString(R.string.app_name))
        )

        v.description.setText(R.string.biometric_auth_description)
        v.newSessionButton.setText(R.string.biometric_auth_new_session)
        v.newSessionButton.setOnClickListener {
            biometricNewSessionButtonClicked()
            dismiss()
        }

        v.fingerprintIcon.setImageResource(R.drawable.ic_fingerprint)
        return v
    }

    override fun onResume() {
        super.onResume()

        resetErrorText()

        handler = BiometricAuthenticationHandler(requireContext(), this)
        handler?.startAuthentication()
    }

    override fun onPause() {
        super.onPause()

        handler?.stopListening()
        handler = null
    }

    private fun biometricNewSessionButtonClicked() {
        requireComponents.tabsUseCases.removePrivateTabs()
        dismiss()
    }

    fun displayError(errorText: String) {
        // Display the error text
        biometricErrorText.text = errorText
        biometricErrorText.setTextColor(
            ContextCompat.getColor(
                biometricErrorText.context,
                R.color.photonRed50
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
        biometricErrorText.text = ""
    }

    companion object {
        const val FRAGMENT_TAG = "biometric-dialog-fragment"
    }
}
