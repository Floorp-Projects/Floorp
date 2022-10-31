/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.dialog

import android.annotation.SuppressLint
import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import android.os.Parcelable
import android.view.LayoutInflater
import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.Companion.PRIVATE
import androidx.appcompat.app.AlertDialog
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.concept.engine.prompt.Choice
import mozilla.components.feature.prompts.R

private const val KEY_CHOICES = "KEY_CHOICES"
private const val KEY_DIALOG_TYPE = "KEY_DIALOG_TYPE"

/**
 * [android.support.v4.app.DialogFragment] implementation to display choice(options,optgroup and menu)
 * web content in native dialogs.
 */
internal class ChoiceDialogFragment : PromptDialogFragment() {

    internal val choices: Array<Choice> by lazy {
        safeArguments.getParcelableArray(KEY_CHOICES)!!.toArrayOfChoices()
    }

    @VisibleForTesting
    internal val dialogType: Int by lazy { safeArguments.getInt(KEY_DIALOG_TYPE) }

    internal val isSingleChoice get() = dialogType == SINGLE_CHOICE_DIALOG_TYPE

    internal val isMenuChoice get() = dialogType == MENU_CHOICE_DIALOG_TYPE

    internal val mapSelectChoice by lazy { HashMap<Choice, Choice>() }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        return when (dialogType) {
            SINGLE_CHOICE_DIALOG_TYPE -> createSingleChoiceDialog()
            MULTIPLE_CHOICE_DIALOG_TYPE -> createMultipleChoiceDialog()
            MENU_CHOICE_DIALOG_TYPE -> createSingleChoiceDialog()
            else -> throw IllegalArgumentException(" $dialogType is not a valid choice dialog type")
        }
    }

    companion object {
        fun newInstance(
            choices: Array<Choice>,
            sessionId: String,
            promptRequestUID: String,
            shouldDismissOnLoad: Boolean,
            dialogType: Int,
        ): ChoiceDialogFragment {
            val fragment = ChoiceDialogFragment()
            val arguments = fragment.arguments ?: Bundle()

            with(arguments) {
                putParcelableArray(KEY_CHOICES, choices)
                putString(KEY_SESSION_ID, sessionId)
                putString(KEY_PROMPT_UID, promptRequestUID)
                putBoolean(KEY_SHOULD_DISMISS_ON_LOAD, shouldDismissOnLoad)
                putInt(KEY_DIALOG_TYPE, dialogType)
            }

            fragment.arguments = arguments

            return fragment
        }

        const val SINGLE_CHOICE_DIALOG_TYPE = 0
        const val MULTIPLE_CHOICE_DIALOG_TYPE = 1
        const val MENU_CHOICE_DIALOG_TYPE = 2
    }

    @SuppressLint("InflateParams")
    internal fun createDialogContentView(inflater: LayoutInflater): View {
        val index = choices.indexOfFirst { it.selected }
        val view = inflater.inflate(R.layout.mozac_feature_choice_dialogs, null)
        view.findViewById<RecyclerView>(R.id.recyclerView).apply {
            layoutManager = LinearLayoutManager(context, RecyclerView.VERTICAL, false).also {
                it.scrollToPosition(index)
            }
            adapter = ChoiceAdapter(this@ChoiceDialogFragment, inflater)
        }
        return view
    }

    fun onSelect(selectedChoice: Choice) {
        feature?.onConfirm(sessionId, promptRequestUID, selectedChoice)
        dismiss()
    }

    override fun onCancel(dialog: DialogInterface) {
        super.onCancel(dialog)
        feature?.onCancel(sessionId, promptRequestUID)
    }

    private fun createSingleChoiceDialog(): AlertDialog {
        val builder = AlertDialog.Builder(requireContext())
        val inflater = LayoutInflater.from(requireContext())
        val view = createDialogContentView(inflater)

        return builder.setView(view)
            .setOnDismissListener {
                feature?.onCancel(sessionId, promptRequestUID)
            }.create()
    }

    private fun createMultipleChoiceDialog(): AlertDialog {
        val builder = AlertDialog.Builder(requireContext())
        val inflater = LayoutInflater.from(requireContext())
        val view = createDialogContentView(inflater)

        return builder.setView(view)
            .setNegativeButton(R.string.mozac_feature_prompts_cancel) { _, _ ->
                feature?.onCancel(sessionId, promptRequestUID)
            }
            .setPositiveButton(R.string.mozac_feature_prompts_ok) { _, _ ->
                feature?.onConfirm(sessionId, promptRequestUID, mapSelectChoice.keys.toTypedArray())
            }.setOnDismissListener {
                feature?.onCancel(sessionId, promptRequestUID)
            }.create()
    }
}

@Suppress("UNCHECKED_CAST")
@VisibleForTesting(otherwise = PRIVATE)
internal fun Array<Parcelable>.toArrayOfChoices(): Array<Choice> {
    return if (this.isArrayOf<Choice>()) {
        this as Array<Choice>
    } else {
        Array(this.size) { index ->
            this[index] as Choice
        }
    }
}
