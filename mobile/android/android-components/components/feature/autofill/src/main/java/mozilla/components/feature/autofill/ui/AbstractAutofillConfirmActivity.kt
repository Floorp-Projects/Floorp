/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.ui

import android.app.Dialog
import android.app.assist.AssistStructure
import android.content.DialogInterface
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.service.autofill.Dataset
import android.view.autofill.AutofillManager
import androidx.annotation.RequiresApi
import androidx.appcompat.app.AlertDialog
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.FragmentActivity
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.runBlocking
import mozilla.components.feature.autofill.AutofillConfiguration
import mozilla.components.feature.autofill.R
import mozilla.components.feature.autofill.facts.emitAutofillConfirmationFact
import mozilla.components.feature.autofill.handler.EXTRA_LOGIN_ID
import mozilla.components.feature.autofill.handler.FillRequestHandler
import mozilla.components.feature.autofill.structure.toRawStructure

/**
 * Activity responsible for asking the user to confirm before autofilling a third-party app. It is
 * shown in situations where the authenticity of an application could not be confirmed automatically
 * with "Digital Asset Links".
 */
@RequiresApi(Build.VERSION_CODES.O)
abstract class AbstractAutofillConfirmActivity : FragmentActivity() {
    abstract val configuration: AutofillConfiguration

    private var dataset: Deferred<Dataset?>? = null
    private val fillHandler by lazy { FillRequestHandler(context = this, configuration) }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val structure: AssistStructure? = intent.getParcelableExtra(AutofillManager.EXTRA_ASSIST_STRUCTURE)
        val loginId = intent.getStringExtra(EXTRA_LOGIN_ID)
        if (loginId == null) {
            cancel()
            return
        }
        val imeSpec = intent.getImeSpec()
        // While the user is asked to confirm, we already try to build the fill response asynchronously.
        val rawStructure = structure?.toRawStructure()
        if (rawStructure != null) {
            dataset = lifecycleScope.async(Dispatchers.IO) {
                val builder = fillHandler.handleConfirmation(rawStructure, loginId)
                builder?.build(this@AbstractAutofillConfirmActivity, configuration, imeSpec)
            }
        }

        if (savedInstanceState == null) {
            val fragment = AutofillConfirmFragment()
            fragment.show(supportFragmentManager, "confirm_fragment")
        }
    }

    /**
     * Confirms the autofill request and returns the credentials to the autofill framework.
     */
    internal fun confirm() {
        val replyIntent = Intent().apply {
            // At this point it should be safe to block since the fill response should be ready once
            // the user has authenticated.
            runBlocking { putExtra(AutofillManager.EXTRA_AUTHENTICATION_RESULT, dataset?.await()) }
        }

        emitAutofillConfirmationFact(confirmed = true)

        setResult(RESULT_OK, replyIntent)
        finish()
    }

    /**
     * Cancels the autofill request.
     */
    internal fun cancel() {
        dataset?.cancel()

        emitAutofillConfirmationFact(confirmed = false)

        setResult(RESULT_CANCELED)
        finish()
    }
}

@RequiresApi(Build.VERSION_CODES.O)
internal class AutofillConfirmFragment : DialogFragment() {
    private val configuration: AutofillConfiguration
        get() = getConfirmActivity().configuration

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        return AlertDialog.Builder(requireContext())
            .setTitle(
                getString(R.string.mozac_feature_autofill_confirmation_title),
            )
            .setMessage(
                getString(R.string.mozac_feature_autofill_confirmation_authenticity, configuration.applicationName),
            )
            .setPositiveButton(R.string.mozac_feature_autofill_confirmation_yes) { _, _ -> confirmRequest() }
            .setNegativeButton(R.string.mozac_feature_autofill_confirmation_no) { _, _ -> cancelRequest() }
            .create()
    }

    override fun onDismiss(dialog: DialogInterface) {
        super.onDismiss(dialog)
        cancelRequest()
    }

    private fun confirmRequest() {
        getConfirmActivity()
            .confirm()
    }

    private fun cancelRequest() {
        getConfirmActivity()
            .cancel()
    }

    private fun getConfirmActivity(): AbstractAutofillConfirmActivity {
        return requireActivity() as AbstractAutofillConfirmActivity
    }
}
