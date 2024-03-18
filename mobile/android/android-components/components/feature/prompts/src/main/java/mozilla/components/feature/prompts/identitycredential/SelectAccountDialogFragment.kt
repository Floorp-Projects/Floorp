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
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material.MaterialTheme
import androidx.compose.material.darkColors
import androidx.compose.material.lightColors
import androidx.compose.ui.platform.ComposeView
import mozilla.components.concept.identitycredential.Account
import mozilla.components.concept.identitycredential.Provider
import mozilla.components.feature.prompts.dialog.KEY_PROMPT_UID
import mozilla.components.feature.prompts.dialog.KEY_SESSION_ID
import mozilla.components.feature.prompts.dialog.KEY_SHOULD_DISMISS_ON_LOAD
import mozilla.components.feature.prompts.dialog.PromptDialogFragment
import mozilla.components.support.utils.ext.getParcelableArrayListCompat
import mozilla.components.support.utils.ext.getParcelableCompat

private const val KEY_ACCOUNTS = "KEY_ACCOUNTS"
private const val KEY_PROVIDER = "KEY_PROVIDER"

/**
 * A Federated Credential Management dialog for selecting an account.
 */
internal class SelectAccountDialogFragment : PromptDialogFragment() {

    internal val accounts: List<Account> by lazy {
        safeArguments.getParcelableArrayListCompat(KEY_ACCOUNTS, Account::class.java) ?: emptyList()
    }

    private var colorsProvider: DialogColorsProvider = DialogColors.defaultProvider()

    internal val provider: Provider by lazy {
        requireNotNull(
            safeArguments.getParcelableCompat(
                KEY_PROVIDER,
                Provider::class.java,
            ),
        )
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
                val colors = if (isSystemInDarkTheme()) darkColors() else lightColors()
                MaterialTheme(colors) {
                    SelectAccountDialog(
                        provider = provider,
                        accounts = accounts,
                        colors = colorsProvider.provideColors(),
                        onAccountClick = ::onAccountChange,
                    )
                }
            }
        }
    }

    /**
     * Called when a new [Provider] is selected by the user.
     */
    @VisibleForTesting
    internal fun onAccountChange(account: Account) {
        feature?.onConfirm(sessionId, promptRequestUID, account)
        dismiss()
    }

    companion object {

        /**
         * A builder method for creating a [SelectAccountDialogFragment]
         * @param sessionId The id of the session for which this dialog will be created.
         * @param promptRequestUID Identifier of the [PromptRequest] for which this dialog is shown.
         * @param accounts The list of available accounts.
         * @param provider The provider on which the user is logging in.
         * @param shouldDismissOnLoad Whether or not the dialog should automatically be dismissed
         * when a new page is loaded.
         * @param colorsProvider Provides [DialogColors] that define the colors in the Dialog
         */
        fun newInstance(
            sessionId: String,
            promptRequestUID: String,
            accounts: List<Account>,
            provider: Provider,
            shouldDismissOnLoad: Boolean,
            colorsProvider: DialogColorsProvider,
        ) = SelectAccountDialogFragment().apply {
            arguments = (arguments ?: Bundle()).apply {
                putString(KEY_SESSION_ID, sessionId)
                putString(KEY_PROMPT_UID, promptRequestUID)
                putBoolean(KEY_SHOULD_DISMISS_ON_LOAD, shouldDismissOnLoad)
                putParcelableArrayList(KEY_ACCOUNTS, ArrayList(accounts))
                putParcelable(KEY_PROVIDER, provider)
            }
            this.colorsProvider = colorsProvider
        }
    }
}
