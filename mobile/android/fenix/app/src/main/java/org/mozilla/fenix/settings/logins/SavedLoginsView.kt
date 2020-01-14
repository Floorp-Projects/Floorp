/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.core.view.isVisible
import androidx.recyclerview.widget.LinearLayoutManager
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.component_saved_logins.view.*
import org.mozilla.fenix.R

/**
 * Interface for the SavedLoginsViewInteractor. This interface is implemented by objects that want
 * to respond to user interaction on the SavedLoginsView
 */
interface SavedLoginsViewInteractor {
    /**
     * Called whenever one item is clicked
     */
    fun itemClicked(item: SavedLoginsItem)
}

/**
 * View that contains and configures the Saved Logins List
 */
class SavedLoginsView(
    override val containerView: ViewGroup,
    val interactor: SavedLoginsInteractor
) : LayoutContainer {

    val view: FrameLayout = LayoutInflater.from(containerView.context)
        .inflate(R.layout.component_saved_logins, containerView, true)
        .findViewById(R.id.saved_logins_wrapper)

    private val loginsAdapter = SavedLoginsAdapter(interactor)

    init {
        view.saved_logins_list.apply {
            adapter = loginsAdapter
            layoutManager = LinearLayoutManager(containerView.context)
        }
    }

    fun update(state: SavedLoginsFragmentState) {
        view.saved_logins_list.isVisible = state.items.isNotEmpty()
        loginsAdapter.submitList(state.items)
    }
}
