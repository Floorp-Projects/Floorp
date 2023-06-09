/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.identitycredential

import android.annotation.SuppressLint
import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AlertDialog
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.concept.identitycredential.Provider
import mozilla.components.feature.prompts.R
import mozilla.components.feature.prompts.dialog.KEY_PROMPT_UID
import mozilla.components.feature.prompts.dialog.KEY_SESSION_ID
import mozilla.components.feature.prompts.dialog.KEY_SHOULD_DISMISS_ON_LOAD
import mozilla.components.feature.prompts.dialog.PromptDialogFragment
import mozilla.components.support.utils.ext.getParcelableArrayListCompat
import mozilla.components.ui.widgets.withCenterAlignedButtons

private const val KEY_PROVIDERS = "KEY_PROVIDERS"

/**
 * A Federated Credential Management dialog for selecting a provider.
 */
internal class SelectProviderDialogFragment : PromptDialogFragment() {

    private lateinit var listAdapter: BasicProviderAdapter

    internal val providers: List<Provider> by lazy {
        safeArguments.getParcelableArrayListCompat(KEY_PROVIDERS, Provider::class.java) ?: emptyList()
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog =
        AlertDialog.Builder(requireContext())
            .setCancelable(true)
            .setTitle(R.string.mozac_feature_prompts_identity_credentials_choose_provider)
            .setView(createDialogContentView())
            .create()
            .withCenterAlignedButtons()

    override fun onCancel(dialog: DialogInterface) {
        super.onCancel(dialog)
        feature?.onCancel(sessionId, promptRequestUID)
    }

    @SuppressLint("InflateParams")
    internal fun createDialogContentView(): View {
        val view = LayoutInflater
            .from(requireContext())
            .inflate(R.layout.mozac_feature_prompts_choose_identity_provider_dialogs, null)

        setupRecyclerView(view)
        return view
    }

    private fun setupRecyclerView(view: View) {
        listAdapter = BasicProviderAdapter(this::onProviderChange)
        view.findViewById<RecyclerView>(R.id.recyclerView).apply {
            layoutManager = LinearLayoutManager(context, RecyclerView.VERTICAL, false)
            adapter = listAdapter
        }
        listAdapter.submitList(providers)
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
