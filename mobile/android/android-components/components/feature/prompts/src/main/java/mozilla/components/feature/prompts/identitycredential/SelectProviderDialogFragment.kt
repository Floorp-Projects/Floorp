/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.identitycredential

import android.annotation.SuppressLint
import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AlertDialog
import androidx.compose.ui.platform.ComposeView
import mozilla.components.concept.identitycredential.Provider
import mozilla.components.feature.prompts.dialog.KEY_PROMPT_UID
import mozilla.components.feature.prompts.dialog.KEY_SESSION_ID
import mozilla.components.feature.prompts.dialog.KEY_SHOULD_DISMISS_ON_LOAD
import mozilla.components.feature.prompts.dialog.PromptDialogFragment
import mozilla.components.support.utils.ext.getParcelableArrayListCompat

private const val KEY_PROVIDERS = "KEY_PROVIDERS"

/**
 * A Federated Credential Management dialog for selecting a provider.
 */
internal class SelectProviderDialogFragment : PromptDialogFragment() {

    private val providers: List<Provider> by lazy {
        safeArguments.getParcelableArrayListCompat(KEY_PROVIDERS, Provider::class.java)
            ?: emptyList()
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog =
        AlertDialog.Builder(requireContext())
            .setCancelable(true)
            .setView(createDialogContentView())
            .create()

    override fun onCancel(dialog: DialogInterface) {
        super.onCancel(dialog)
        feature?.onCancel(sessionId, promptRequestUID)
    }

    @SuppressLint("InflateParams")
    internal fun createDialogContentView(): View {
        return ComposeView(requireContext()).apply {
            setContent {
                SelectProviderDialog(
                    providers = providers,
                    onProviderClick = ::onProviderChange,
                )
            }
        }
    }

    /**
     * Called when a new [Provider] is selected by the user.
     */
    @VisibleForTesting
    internal fun onProviderChange(provider: Provider) {
        feature?.onConfirm(sessionId, promptRequestUID, provider)
        dismiss()
    }

    companion object {

        /**
         * A builder method for creating a [SelectAccountDialogFragment]
         * @param sessionId The id of the session for which this dialog will be created.
         * @param promptRequestUID Identifier of the [PromptRequest] for which this dialog is shown.
         * @param providers The list of available providers.
         * @param shouldDismissOnLoad Whether or not the dialog should automatically be dismissed when a new page is loaded.
         */
        fun newInstance(
            sessionId: String,
            promptRequestUID: String,
            providers: List<Provider>,
            shouldDismissOnLoad: Boolean,
        ) = SelectProviderDialogFragment().apply {
            arguments = (arguments ?: Bundle()).apply {
                putString(KEY_SESSION_ID, sessionId)
                putString(KEY_PROMPT_UID, promptRequestUID)
                putBoolean(KEY_SHOULD_DISMISS_ON_LOAD, shouldDismissOnLoad)
                putParcelableArrayList(KEY_PROVIDERS, ArrayList(providers))
            }
        }
    }
}
