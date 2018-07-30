/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.biometrics

import android.app.DialogFragment
import android.arch.lifecycle.LifecycleObserver
import android.content.DialogInterface
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.support.annotation.RequiresApi
import android.support.v4.content.ContextCompat
import android.support.v7.app.AppCompatDialogFragment
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import kotlinx.android.synthetic.main.biometric_prompt_dialog_content.new_session_button
import kotlinx.android.synthetic.main.biometric_prompt_dialog_content.biometric_error_text
import kotlinx.android.synthetic.main.biometric_prompt_dialog_content.fingerprint_icon
import kotlinx.android.synthetic.main.biometric_prompt_dialog_content.description
import org.mozilla.focus.R

@RequiresApi(api = Build.VERSION_CODES.M)
class BiometricAuthenticationDialogFragment : AppCompatDialogFragment(), LifecycleObserver {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(DialogFragment.STYLE_NORMAL, android.R.style.Theme_Material_Light_Dialog)
    }

    override fun onCancel(dialog: DialogInterface?) {
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
        biometricDialog.setCanceledOnTouchOutside(false)
        biometricDialog.setTitle(
            context!!.getString(
                R.string.biometric_auth_title,
                context!!.getString(R.string.app_name)
            )
        )

        description.setText(R.string.biometric_auth_description)

        new_session_button.setText(R.string.biometric_auth_new_session)
        new_session_button.setOnClickListener {
            newSessionButtonClicked()
            dismiss()
        }

        fingerprint_icon.setImageResource(R.drawable.ic_fingerprint)
        return v
    }

    override fun onResume() {
        super.onResume()
        resetErrorText()
    }

    interface BiometricAuthenticationListener {
        fun onCreateNewSession()
        fun onAuthSuccess()
    }

    fun newSessionButtonClicked() {
        val listener = targetFragment as BiometricAuthenticationListener?
        listener?.onCreateNewSession()
        dismiss()
    }

    fun displayError(errorText: String) {
        // Display the error text
        biometric_error_text.text = errorText
        biometric_error_text.setTextColor(
            ContextCompat.getColor(
                biometric_error_text.context,
                R.color.photonRed50
            )
        )
    }

    fun onAuthenticated() {
        val listener = targetFragment as BiometricAuthenticationListener?
        if (listener != null) {
            listener.onAuthSuccess()
            dismiss()
        }
    }

    fun onFailure() {
        displayError(context!!.getString(R.string.biometric_auth_not_recognized_error))
    }

    private fun resetErrorText() {
        biometric_error_text.text = ""
    }

    companion object {
        val FRAGMENT_TAG = "biometric-dialog-fragment"
    }
}
