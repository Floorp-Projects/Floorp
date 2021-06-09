/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.content.Context
import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.core.content.ContextCompat
import androidx.core.view.isVisible
import kotlinx.android.synthetic.main.component_tabstray2.view.exit_multi_select
import kotlinx.android.synthetic.main.component_tabstray2.view.multiselect_title
import kotlinx.android.synthetic.main.tabstray_multiselect_items.view.*
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.lib.state.helpers.AbstractBinding
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import org.mozilla.fenix.R
import org.mozilla.fenix.tabstray.NavigationInteractor
import org.mozilla.fenix.tabstray.TabsTrayInteractor
import org.mozilla.fenix.tabstray.TabsTrayState
import org.mozilla.fenix.tabstray.TabsTrayStore
import org.mozilla.fenix.tabstray.TabsTrayAction.ExitSelectMode
import org.mozilla.fenix.tabstray.TabsTrayState.Mode
import org.mozilla.fenix.tabstray.TabsTrayState.Mode.Select
import org.mozilla.fenix.tabstray.ext.showWithTheme

/**
 * A binding that shows/hides the multi-select banner of the selected count of tabs.
 *
 * @property context An Android context.
 * @property store The TabsTrayStore instance.
 * @property navInteractor An instance of [NavigationInteractor] for navigating on menu clicks.
 * @property tabsTrayInteractor An instance of [TabsTrayInteractor] for handling deletion.
 * @property containerView The view in the layout that contains all the implicit multi-select
 * views. NB: This parameter is a bit opaque and requires a larger layout refactor to correct.
 * @property backgroundView The background view that we want to alter when changing [Mode].
 * @property showOnSelectViews A variable list of views that will be made visible when in select mode.
 * @property showOnNormalViews A variable list of views that will be made visible when in normal mode.
 */
@OptIn(ExperimentalCoroutinesApi::class)
@Suppress("LongParameterList")
class SelectionBannerBinding(
    private val context: Context,
    private val store: TabsTrayStore,
    private val navInteractor: NavigationInteractor,
    private val tabsTrayInteractor: TabsTrayInteractor,
    private val containerView: View,
    private val backgroundView: View,
    private val showOnSelectViews: VisibilityModifier,
    private val showOnNormalViews: VisibilityModifier
) : AbstractBinding<TabsTrayState>(store) {

    /**
     * A holder of views that will be used by having their [View.setVisibility] modified.
     */
    class VisibilityModifier(vararg val views: View)

    private var isPreviousModeSelect = false

    override fun start() {
        super.start()

        initListeners(containerView)
    }

    override suspend fun onState(flow: Flow<TabsTrayState>) {
        flow.map { it.mode }
            .ifChanged()
            .collect { mode ->
                val isSelectMode = mode is Select

                showOnSelectViews.views.forEach {
                    it.isVisible = isSelectMode
                }

                showOnNormalViews.views.forEach {
                    it.isVisible = isSelectMode.not()
                }

                updateBackgroundColor(isSelectMode)

                updateSelectTitle(isSelectMode, mode.selectedTabs.size)

                isPreviousModeSelect = isSelectMode
            }
    }

    private fun initListeners(containerView: View) {
        containerView.share_multi_select.setOnClickListener {
            navInteractor.onShareTabs(store.state.mode.selectedTabs)
        }

        containerView.collect_multi_select.setOnClickListener {
            navInteractor.onSaveToCollections(store.state.mode.selectedTabs)
        }

        containerView.exit_multi_select.setOnClickListener {
            store.dispatch(ExitSelectMode)
        }

        containerView.menu_multi_select.setOnClickListener { anchor ->
            val menu = SelectionMenuIntegration(
                context,
                store,
                navInteractor,
                tabsTrayInteractor
            ).build()

            menu.showWithTheme(anchor)
        }
    }

    @VisibleForTesting
    private fun updateBackgroundColor(isSelectMode: Boolean) {
        // memoize to avoid setting the background unnecessarily.
        if (isPreviousModeSelect != isSelectMode) {
            val colorResource = if (isSelectMode) {
                R.color.accent_normal_theme
            } else {
                R.color.foundation_normal_theme
            }

            val color = ContextCompat.getColor(backgroundView.context, colorResource)

            backgroundView.setBackgroundColor(color)
        }
    }

    @VisibleForTesting
    private fun updateSelectTitle(selectedMode: Boolean, tabCount: Int) {
        if (selectedMode) {
            containerView.multiselect_title.text =
                context.getString(R.string.tab_tray_multi_select_title, tabCount)
            containerView.multiselect_title.importantForAccessibility =
                View.IMPORTANT_FOR_ACCESSIBILITY_YES
        }
    }
}
