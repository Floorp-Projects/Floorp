/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.biometrics

import android.app.Dialog
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
import android.widget.Button
import android.widget.ImageView
import android.widget.TextView
import org.mozilla.focus.R

@RequiresApi(api = Build.VERSION_CODES.M)
class BiometricAuthenticationDialogFragment : AppCompatDialogFragment(), LifecycleObserver {
    private var mErrorTextView: TextView? = null

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

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?,
                              savedInstanceState: Bundle?): View? {

        val v = inflater.inflate(R.layout.biometric_prompt_dialog_content, container,
                false)

        val biometricDialog = dialog
        this.isCancelable = true
        biometricDialog.setCanceledOnTouchOutside(false)
        biometricDialog.setTitle(context!!.getString(R.string.biometric_auth_title,
                context!!.getString(R.string.app_name)))

        val description = v.findViewById<TextView>(R.id.description)
        description.setText(R.string.biometric_auth_description)

        val mNewSessionButton = v.findViewById<Button>(R.id.new_session_button)
        mNewSessionButton.setText(R.string.biometric_auth_new_session)
        mNewSessionButton.setOnClickListener {
            newSessionButtonClicked()
            dismiss()
        }

        val mFingerprintContent = v.findViewById<ImageView>(R.id.fingerprint_icon)
        mFingerprintContent.setImageResource(R.drawable.ic_fingerprint)
        mErrorTextView = v.findViewById(R.id.biometric_error_text)

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
        mErrorTextView!!.text = errorText
        mErrorTextView!!.setTextColor(ContextCompat.getColor(mErrorTextView!!.context, R.color.photonRed50))
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
        mErrorTextView!!.text = ""
    }

    companion object {
        val FRAGMENT_TAG = "biometric-dialog-fragment"
    }
}
