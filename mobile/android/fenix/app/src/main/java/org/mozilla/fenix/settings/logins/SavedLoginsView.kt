/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import android.text.method.LinkMovementMethod
import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.core.view.isVisible
import androidx.navigation.NavController
import androidx.recyclerview.widget.LinearLayoutManager
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.component_saved_logins.view.*
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.addUnderline
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.utils.Settings

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

    private val loginsAdapter = LoginsAdapter(interactor)

    init {
        view.saved_logins_list.apply {
            adapter = loginsAdapter
            layoutManager = LinearLayoutManager(containerView.context)
            itemAnimator = null
        }

        with(view.saved_passwords_empty_learn_more) {
            movementMethod = LinkMovementMethod.getInstance()
            addUnderline()
            setOnClickListener { interactor.onLearnMoreClicked() }
        }

        with(view.saved_passwords_empty_message) {
            val appName = context.getString(R.string.app_name)
            text = String.format(
                context.getString(
                    R.string.preferences_passwords_saved_logins_description_empty_text
                ), appName
            )
        }
    }

    fun update(state: LoginsListState) {
        // todo MVI views should not have logic. Needs refactoring.
        if (state.isLoading) {
            view.progress_bar.isVisible = true
        } else {
            view.progress_bar.isVisible = false
            view.saved_logins_list.isVisible = state.loginList.isNotEmpty()
            view.saved_passwords_empty_view.isVisible = state.loginList.isEmpty()
        }
        loginsAdapter.submitList(state.filteredItems)
    }
}

/**
 * Interactor for the saved logins screen
 *
 * @param savedLoginsController [SavedLoginsController] which will be delegated for all users interactions.
 */
class SavedLoginsInteractor(
    private val savedLoginsController: SavedLoginsController
) {
    fun onItemClicked(item: SavedLogin) {
        savedLoginsController.handleItemClicked(item)
    }

    fun onLearnMoreClicked() {
        savedLoginsController.handleLearnMoreClicked()
    }

    fun onSortingStrategyChanged(sortingStrategy: SortingStrategy) {
        savedLoginsController.handleSort(sortingStrategy)
    }
}

/**
 * Controller for the saved logins screen
 *
 * @param store Store used to hold in-memory collection state.
 * @param navController NavController manages app navigation within a NavHost.
 * @param browserNavigator Controller allowing browser navigation to any Uri.
 * @param settings SharedPreferences wrapper for easier usage.
 * @param metrics Controller that handles telemetry events.
 */
class SavedLoginsController(
    private val store: LoginsFragmentStore,
    private val navController: NavController,
    private val browserNavigator: (
        searchTermOrURL: String,
        newTab: Boolean,
        from: BrowserDirection
    ) -> Unit,
    private val settings: Settings,
    private val metrics: MetricController
) {
    fun handleSort(sortingStrategy: SortingStrategy) {
        store.dispatch(LoginsAction.SortLogins(sortingStrategy))
        settings.savedLoginsSortingStrategy = sortingStrategy
    }

    fun handleItemClicked(item: SavedLogin) {
        store.dispatch(LoginsAction.LoginSelected(item))
        metrics.track(Event.OpenOneLogin)
        navController.navigate(
            SavedLoginsFragmentDirections.actionSavedLoginsFragmentToLoginDetailFragment(item.guid)
        )
    }

    fun handleLearnMoreClicked() {
        browserNavigator.invoke(
            SupportUtils.getGenericSumoURLForTopic(SupportUtils.SumoTopic.SYNC_SETUP),
            true,
            BrowserDirection.FromSavedLoginsFragment
        )
    }
}
