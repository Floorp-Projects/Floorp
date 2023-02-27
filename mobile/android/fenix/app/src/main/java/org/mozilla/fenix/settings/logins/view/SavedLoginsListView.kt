/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.view

import android.text.method.LinkMovementMethod
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.core.view.isVisible
import androidx.recyclerview.widget.LinearLayoutManager
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.ComponentSavedLoginsBinding
import org.mozilla.fenix.ext.addUnderline
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.settings.logins.LoginsListState
import org.mozilla.fenix.settings.logins.interactor.SavedLoginsInteractor

/**
 * View that contains and configures the Saved Logins List
 */
class SavedLoginsListView(
    private val containerView: ViewGroup,
    val interactor: SavedLoginsInteractor,
) {
    private val binding = ComponentSavedLoginsBinding.inflate(
        LayoutInflater.from(containerView.context),
        containerView,
        true,
    )

    private val loginsAdapter = LoginsAdapter(interactor)

    init {
        binding.savedLoginsList.apply {
            adapter = loginsAdapter
            layoutManager = LinearLayoutManager(containerView.context)
            itemAnimator = null
        }

        with(binding.savedPasswordsEmptyLearnMore) {
            increaseTapArea(LEARN_MORE_EXTRA_DIPS)
            movementMethod = LinkMovementMethod.getInstance()
            addUnderline()
            setOnClickListener { interactor.onLearnMoreClicked() }
        }

        with(binding.savedPasswordsEmptyMessage) {
            val appName = context.getString(R.string.app_name)
            text = String.format(
                context.getString(
                    R.string.preferences_passwords_saved_logins_description_empty_text,
                ),
                appName,
            )
        }

        binding.addLoginButton.addLoginLayout.setOnClickListener { interactor.onAddLoginClick() }
    }

    fun update(state: LoginsListState) {
        if (state.isLoading) {
            binding.progressBar.isVisible = true
        } else {
            binding.progressBar.isVisible = false
            binding.savedLoginsList.isVisible = state.loginList.isNotEmpty()
            binding.savedPasswordsEmptyView.isVisible = state.loginList.isEmpty()
        }
        loginsAdapter.submitList(state.filteredItems) {
            // Reset scroll position to the first item after submitted list was committed.
            binding.savedLoginsList.scrollToPosition(0)
        }
    }

    companion object {
        private const val LEARN_MORE_EXTRA_DIPS = 24
    }
}
