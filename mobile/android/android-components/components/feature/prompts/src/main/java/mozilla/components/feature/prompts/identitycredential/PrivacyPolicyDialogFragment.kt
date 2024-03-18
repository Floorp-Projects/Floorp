/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.identitycredential

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import android.text.SpannableStringBuilder
import android.text.method.LinkMovementMethod
import android.text.style.ClickableSpan
import android.text.style.URLSpan
import android.view.LayoutInflater
import android.view.View
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.core.text.HtmlCompat
import androidx.core.text.getSpans
import mozilla.components.feature.prompts.R
import mozilla.components.feature.prompts.dialog.AbstractPromptTextDialogFragment
import mozilla.components.feature.prompts.dialog.KEY_MESSAGE
import mozilla.components.feature.prompts.dialog.KEY_PROMPT_UID
import mozilla.components.feature.prompts.dialog.KEY_SESSION_ID
import mozilla.components.feature.prompts.dialog.KEY_SHOULD_DISMISS_ON_LOAD
import mozilla.components.feature.prompts.dialog.KEY_TITLE

internal const val KEY_ICON = "KEY_ICON"

/**
 * [ A Federated Credential Management dialog for showing a privacy policy.
 */
internal class PrivacyPolicyDialogFragment : AbstractPromptTextDialogFragment() {

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val builder = AlertDialog.Builder(requireContext())
            .setTitle(title)
            .setCancelable(true)
            .setPositiveButton(R.string.mozac_feature_prompts_identity_credentials_continue) { _, _ ->
                onConfirmAction(true)
            }
            .setNegativeButton(R.string.mozac_feature_prompts_identity_credentials_cancel) { _, _ ->
                onConfirmAction(false)
            }

        return setMessage(builder)
            .create()
    }

    internal fun setMessage(builder: AlertDialog.Builder): AlertDialog.Builder {
        val inflater = LayoutInflater.from(requireContext())
        val view = inflater.inflate(R.layout.mozac_feature_prompt_simple_text, null)
        val textView = view.findViewById<TextView>(R.id.labelView)
        val text = HtmlCompat.fromHtml(message, HtmlCompat.FROM_HTML_MODE_COMPACT)

        val spannableStringBuilder = SpannableStringBuilder(text)
        spannableStringBuilder.getSpans<URLSpan>().forEach { link ->
            addActionToLinks(spannableStringBuilder, link)
        }
        textView.text = spannableStringBuilder
        textView.movementMethod = LinkMovementMethod.getInstance()

        builder.setView(view)

        return builder
    }

    override fun onCancel(dialog: DialogInterface) {
        super.onCancel(dialog)
        feature?.onCancel(sessionId, promptRequestUID)
    }

    private fun addActionToLinks(
        spannableStringBuilder: SpannableStringBuilder,
        link: URLSpan,
    ) {
        val start = spannableStringBuilder.getSpanStart(link)
        val end = spannableStringBuilder.getSpanEnd(link)
        val flags = spannableStringBuilder.getSpanFlags(link)
        val clickable: ClickableSpan = object : ClickableSpan() {
            override fun onClick(view: View) {
                view.setOnClickListener {
                    dismiss()
                    feature?.onOpenLink(link.url)
                }
            }
        }
        spannableStringBuilder.setSpan(clickable, start, end, flags)
        spannableStringBuilder.removeSpan(link)
    }

    private fun onConfirmAction(confirmed: Boolean) {
        feature?.onConfirm(sessionId, promptRequestUID, confirmed)
    }

    companion object {
        /**
         * A builder method for creating a [PrivacyPolicyDialogFragment]
         * @param sessionId to create the dialog.
         * @param promptRequestUID identifier of the [PromptRequest] for which this dialog is shown.
         * @param shouldDismissOnLoad whether or not the dialog should automatically be dismissed
         * when a new page is loaded.
         * @param title the title of the dialog.
         * @param message the message of the dialog.
         * @param icon an icon of the provider.
         */
        fun newInstance(
            sessionId: String,
            promptRequestUID: String,
            shouldDismissOnLoad: Boolean,
            title: String,
            message: String,
            icon: String?,
        ): PrivacyPolicyDialogFragment {
            val fragment = PrivacyPolicyDialogFragment()
            val arguments = fragment.arguments ?: Bundle()

            with(arguments) {
                putString(KEY_SESSION_ID, sessionId)
                putString(KEY_PROMPT_UID, promptRequestUID)
                putString(KEY_TITLE, title)
                putString(KEY_MESSAGE, message)
                putBoolean(KEY_SHOULD_DISMISS_ON_LOAD, shouldDismissOnLoad)
                putString(KEY_ICON, icon)
            }
            fragment.arguments = arguments
            return fragment
        }
    }
}
