/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.creditcard

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.TextView
import androidx.annotation.VisibleForTesting
import androidx.appcompat.widget.AppCompatTextView
import androidx.core.view.isVisible
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialog
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.concept.storage.CreditCardEntry
import mozilla.components.concept.storage.CreditCardValidationDelegate.Result
import mozilla.components.feature.prompts.R
import mozilla.components.feature.prompts.dialog.KEY_PROMPT_UID
import mozilla.components.feature.prompts.dialog.KEY_SESSION_ID
import mozilla.components.feature.prompts.dialog.KEY_SHOULD_DISMISS_ON_LOAD
import mozilla.components.feature.prompts.dialog.PromptDialogFragment
import mozilla.components.feature.prompts.facts.emitCreditCardAutofillCreatedFact
import mozilla.components.feature.prompts.facts.emitCreditCardAutofillUpdatedFact
import mozilla.components.support.ktx.android.view.toScope
import mozilla.components.support.utils.creditCardIssuerNetwork

private const val KEY_CREDIT_CARD = "KEY_CREDIT_CARD"

/**
 * [android.support.v4.app.DialogFragment] implementation to display a dialog that allows
 * user to save a new credit card or update an existing credit card.
 */
internal class CreditCardSaveDialogFragment : PromptDialogFragment() {

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal val creditCard by lazy { safeArguments.getParcelable<CreditCardEntry>(KEY_CREDIT_CARD)!! }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var confirmResult: Result = Result.CanBeCreated

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        return BottomSheetDialog(requireContext(), R.style.MozDialogStyle).apply {
            setCancelable(true)
            setOnShowListener {
                val bottomSheet =
                    findViewById<View>(com.google.android.material.R.id.design_bottom_sheet) as FrameLayout
                val behavior = BottomSheetBehavior.from(bottomSheet)
                behavior.state = BottomSheetBehavior.STATE_EXPANDED
            }
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        return LayoutInflater.from(requireContext()).inflate(
            R.layout.mozac_feature_prompt_save_credit_card_prompt,
            container,
            false,
        )
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        view.findViewById<ImageView>(R.id.credit_card_logo)
            .setImageResource(creditCard.cardType.creditCardIssuerNetwork().icon)

        view.findViewById<TextView>(R.id.credit_card_number).text = creditCard.obfuscatedCardNumber
        view.findViewById<TextView>(R.id.credit_card_expiration_date).text = creditCard.expiryDate

        view.findViewById<Button>(R.id.save_confirm).setOnClickListener {
            feature?.onConfirm(
                sessionId = sessionId,
                promptRequestUID = promptRequestUID,
                value = creditCard,
            )
            dismiss()
            emitSaveUpdateFact()
        }

        view.findViewById<Button>(R.id.save_cancel).setOnClickListener {
            feature?.onCancel(
                sessionId = sessionId,
                promptRequestUID = promptRequestUID,
            )
            dismiss()
        }

        updateUI(view)
    }

    /**
     * Emit the save or update fact based on the confirm action for the credit card.
     */
    @VisibleForTesting
    internal fun emitSaveUpdateFact() {
        when (confirmResult) {
            is Result.CanBeCreated -> {
                emitCreditCardAutofillCreatedFact()
            }
            is Result.CanBeUpdated -> {
                emitCreditCardAutofillUpdatedFact()
            }
        }
    }

    override fun onCancel(dialog: DialogInterface) {
        super.onCancel(dialog)
        feature?.onCancel(
            sessionId = sessionId,
            promptRequestUID = promptRequestUID,
        )
    }

    /**
     * Updates the dialog based on whether a save or update credit card state should be displayed.
     */
    private fun updateUI(view: View) = view.toScope().launch(IO) {
        val validationDelegate = feature?.creditCardValidationDelegate ?: return@launch
        confirmResult = validationDelegate.shouldCreateOrUpdate(creditCard)

        withContext(Main) {
            when (confirmResult) {
                is Result.CanBeCreated -> setViewText(
                    view = view,
                    header = requireContext().getString(R.string.mozac_feature_prompts_save_credit_card_prompt_title),
                    cancelButtonText = requireContext().getString(R.string.mozac_feature_prompt_not_now),
                    confirmButtonText = requireContext().getString(R.string.mozac_feature_prompt_save_confirmation),
                )
                is Result.CanBeUpdated -> setViewText(
                    view = view,
                    header = requireContext().getString(R.string.mozac_feature_prompts_update_credit_card_prompt_title),
                    cancelButtonText = requireContext().getString(R.string.mozac_feature_prompts_cancel),
                    confirmButtonText = requireContext().getString(R.string.mozac_feature_prompt_update_confirmation),
                    showMessageBody = false,
                )
            }
        }
    }

    /**
     * Updates the header and button text in the dialog.
     *
     * @param view The view associated with the dialog.
     * @param header The header text to be displayed.
     * @param cancelButtonText The cancel button text to be displayed.
     * @param confirmButtonText The confirm button text to be displayed.
     * @param showMessageBody Whether or not to show the dialog message body text.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun setViewText(
        view: View,
        header: String,
        cancelButtonText: String,
        confirmButtonText: String,
        showMessageBody: Boolean = true,
    ) {
        view.findViewById<AppCompatTextView>(R.id.save_credit_card_message).isVisible =
            showMessageBody
        view.findViewById<AppCompatTextView>(R.id.save_credit_card_header).text = header
        view.findViewById<Button>(R.id.save_cancel).text = cancelButtonText
        view.findViewById<Button>(R.id.save_confirm).text = confirmButtonText
    }

    companion object {
        fun newInstance(
            sessionId: String,
            promptRequestUID: String,
            shouldDismissOnLoad: Boolean,
            creditCard: CreditCardEntry,
        ): CreditCardSaveDialogFragment {
            val fragment = CreditCardSaveDialogFragment()
            val arguments = fragment.arguments ?: Bundle()

            with(arguments) {
                putString(KEY_SESSION_ID, sessionId)
                putString(KEY_PROMPT_UID, promptRequestUID)
                putBoolean(KEY_SHOULD_DISMISS_ON_LOAD, shouldDismissOnLoad)
                putParcelable(KEY_CREDIT_CARD, creditCard)
            }

            fragment.arguments = arguments
            return fragment
        }
    }
}
