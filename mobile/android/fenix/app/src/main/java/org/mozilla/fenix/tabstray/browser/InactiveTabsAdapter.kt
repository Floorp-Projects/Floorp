/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.view.ViewGroup
import androidx.compose.ui.platform.ComposeView
import androidx.lifecycle.LifecycleOwner
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.tabstray.TabsTrayState
import org.mozilla.fenix.tabstray.TabsTrayStore

/**
 * The adapter for displaying the section of inactive tabs.
 *
 * @param lifecycleOwner [LifecycleOwner] to which the Composable will be tied to.
 * @param tabsTrayStore [TabsTrayStore] used to listen for changes to [TabsTrayState.inactiveTabs].
 * @param interactor [InactiveTabsInteractor] used to respond to interactions with the inactive tabs header
 * and the auto close dialog.
 * @property featureName [String] representing the name of the inactive tabs feature for telemetry reporting.
 */
@Suppress("LongParameterList")
class InactiveTabsAdapter(
    private val lifecycleOwner: LifecycleOwner,
    private val tabsTrayStore: TabsTrayStore,
    private val interactor: InactiveTabsInteractor,
    override val featureName: String,
) : RecyclerView.Adapter<InactiveTabViewHolder>(), FeatureNameHolder {

    override fun getItemCount(): Int = 1

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): InactiveTabViewHolder {
        return InactiveTabViewHolder(
            composeView = ComposeView(parent.context),
            lifecycleOwner = lifecycleOwner,
            tabsTrayStore = tabsTrayStore,
            interactor = interactor,
        )
    }

    override fun onBindViewHolder(holder: InactiveTabViewHolder, position: Int) {
        // no-op. This ViewHolder receives the TabsTrayStore as argument and will observe that
        // without the need for us to manually update here for the data to be displayed.
    }

    override fun getItemViewType(position: Int): Int = InactiveTabViewHolder.LAYOUT_ID

    override fun onViewRecycled(holder: InactiveTabViewHolder) {
        // no op
        // This previously called "composeView.disposeComposition" which would have the
        // entire Composable destroyed and recreated when this View is scrolled off or on screen again.
        // This View already listens and maps store updates. Avoid creating and binding new Views.
        // The composition will live until the ViewTreeLifecycleOwner to which it's attached to is destroyed.
    }
}
