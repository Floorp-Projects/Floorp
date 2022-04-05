/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.view.View
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.unit.dp
import androidx.lifecycle.LifecycleOwner
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.lib.state.ext.observeAsComposableState
import org.mozilla.fenix.R
import org.mozilla.fenix.components.components
import org.mozilla.fenix.compose.CollectionsPlaceholder
import org.mozilla.fenix.compose.ComposeViewHolder
import org.mozilla.fenix.home.sessioncontrol.CollectionInteractor

/**
 * [RecyclerView.ComposeViewHolder] for displaying a message detailing the collections feature and
 * allowing users to easily start creating their first.
 *
 * @param composeView [ComposeView] which will be populated with Jetpack Compose UI content.
 * @param viewLifecycleOwner [LifecycleOwner] to which this Composable will be tied to.
 * @param interactor [CollectionInteractor] callback for user interaction.
 */
class NoCollectionsMessageViewHolder(
    composeView: ComposeView,
    viewLifecycleOwner: LifecycleOwner,
    private val interactor: CollectionInteractor
) : ComposeViewHolder(composeView, viewLifecycleOwner) {

    init {
        val horizontalPadding =
            composeView.resources.getDimensionPixelSize(R.dimen.home_item_horizontal_margin)
        composeView.setPadding(horizontalPadding, 0, horizontalPadding, 0)
    }

    companion object {
        val LAYOUT_ID = View.generateViewId()
    }

    @Composable
    override fun Content() {
        val normalTabsState = components.core.store.observeAsComposableState {
            state ->
            state.normalTabs
        }.value ?: emptyList()

        Column {
            Spacer(modifier = Modifier.height(40.dp))

            CollectionsPlaceholder(
                showAddToCollectionButton = normalTabsState.isNotEmpty(),
                onAddTabsToCollectionButtonClick = interactor::onAddTabsToCollectionTapped,
                onRemovePlaceholderClick = interactor::onRemoveCollectionsPlaceholder
            )

            Spacer(modifier = Modifier.height(12.dp))
        }
    }
}
