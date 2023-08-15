/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import android.content.res.Configuration
import android.graphics.drawable.ColorDrawable
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import androidx.navigation.fragment.findNavController
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.databinding.FragmentHomeBinding
import org.mozilla.fenix.ext.hideToolbar
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.home.HomeMenuView
import org.mozilla.fenix.home.PrivateBrowsingButtonView
import org.mozilla.fenix.home.TabCounterView
import org.mozilla.fenix.home.ToolbarView
import org.mozilla.fenix.home.privatebrowsing.controller.DefaultPrivateBrowsingController
import org.mozilla.fenix.home.toolbar.DefaultToolbarController
import org.mozilla.fenix.home.toolbar.SearchSelectorBinding
import org.mozilla.fenix.home.toolbar.SearchSelectorMenuBinding
import org.mozilla.fenix.onboarding.controller.DefaultOnboardingController
import org.mozilla.fenix.onboarding.interactor.DefaultOnboardingInteractor
import org.mozilla.fenix.onboarding.view.OnboardingView
import org.mozilla.fenix.search.toolbar.DefaultSearchSelectorController
import org.mozilla.fenix.search.toolbar.SearchSelectorMenu
import java.lang.ref.WeakReference

/**
 * Displays the first run onboarding screen.
 */
class OnboardingFragment : Fragment() {

    private var _binding: FragmentHomeBinding? = null
    private val binding get() = _binding!!

    private val searchSelectorMenu by lazy {
        SearchSelectorMenu(
            context = requireContext(),
            interactor = interactor,
        )
    }

    private val store: BrowserStore
        get() = requireComponents.core.store
    private val browsingModeManager
        get() = (activity as HomeActivity).browsingModeManager

    private var _interactor: DefaultOnboardingInteractor? = null
    private val interactor: DefaultOnboardingInteractor
        get() = _interactor!!

    private var onboardingView: OnboardingView? = null
    private var homeMenuView: HomeMenuView? = null
    private var tabCounterView: TabCounterView? = null
    private var toolbarView: ToolbarView? = null

    private lateinit var onboardingStore: OnboardingStore
    private lateinit var onboardingAccountObserver: OnboardingAccountObserver

    private val searchSelectorBinding = ViewBoundFeatureWrapper<SearchSelectorBinding>()
    private val searchSelectorMenuBinding = ViewBoundFeatureWrapper<SearchSelectorMenuBinding>()

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        _binding = FragmentHomeBinding.inflate(inflater, container, false)

        val activity = activity as HomeActivity

        onboardingAccountObserver = OnboardingAccountObserver(
            context = requireContext(),
            dispatchChanges = ::dispatchOnboardingStateChanges,
        )

        onboardingStore = StoreProvider.get(this) {
            OnboardingStore(
                initialState = OnboardingFragmentState(
                    onboardingState = onboardingAccountObserver.getOnboardingState(),
                ),
            )
        }

        _interactor = DefaultOnboardingInteractor(
            controller = DefaultOnboardingController(
                activity = activity,
                navController = findNavController(),
                onboarding = requireComponents.fenixOnboarding,
                crashReporter = requireComponents.analytics.crashReporter,
            ),
            privateBrowsingController = DefaultPrivateBrowsingController(
                activity = activity,
                appStore = requireComponents.appStore,
                navController = findNavController(),
            ),
            searchSelectorController = DefaultSearchSelectorController(
                activity = activity,
                navController = findNavController(),
            ),
            toolbarController = DefaultToolbarController(
                activity = activity,
                store = store,
                navController = findNavController(),
            ),
        )

        toolbarView = ToolbarView(
            binding = binding,
            context = requireContext(),
            interactor = interactor,
        )

        onboardingView = OnboardingView(
            containerView = binding.sessionControlRecyclerView,
            interactor = interactor,
        )

        activity.themeManager.applyStatusBarTheme(activity)

        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        consumeFrom(onboardingStore) { state ->
            onboardingView?.update(state.onboardingState, requireComponents.fenixOnboarding.config)
        }

        homeMenuView = HomeMenuView(
            view = view,
            context = view.context,
            lifecycleOwner = viewLifecycleOwner,
            homeActivity = activity as HomeActivity,
            navController = findNavController(),
            menuButton = WeakReference(binding.menuButton),
            hideOnboardingIfNeeded = { interactor.onFinishOnboarding(focusOnAddressBar = false) },
        ).also { it.build() }

        tabCounterView = TabCounterView(
            context = requireContext(),
            browsingModeManager = browsingModeManager,
            navController = findNavController(),
            tabCounter = binding.tabButton,
        )

        consumeFrom(store) {
            tabCounterView?.update(it)
        }

        toolbarView?.build()

        PrivateBrowsingButtonView(
            button = binding.privateBrowsingButton,
            browsingModeManager = browsingModeManager,
            onClick = { mode ->
                interactor.onPrivateModeButtonClicked(mode, userHasBeenOnboarded = false)
            },
        )

        searchSelectorBinding.set(
            feature = SearchSelectorBinding(
                context = view.context,
                binding = binding,
                browserStore = store,
                searchSelectorMenu = searchSelectorMenu,
            ),
            owner = viewLifecycleOwner,
            view = binding.root,
        )

        searchSelectorMenuBinding.set(
            feature = SearchSelectorMenuBinding(
                context = view.context,
                interactor = interactor,
                searchSelectorMenu = searchSelectorMenu,
                browserStore = store,
            ),
            owner = viewLifecycleOwner,
            view = view,
        )
    }

    override fun onResume() {
        super.onResume()

        if (browsingModeManager.mode == BrowsingMode.Private) {
            activity?.window?.setBackgroundDrawableResource(R.drawable.private_home_background_gradient)
        }

        hideToolbar()
    }

    override fun onPause() {
        super.onPause()

        if (browsingModeManager.mode == BrowsingMode.Private) {
            activity?.window?.setBackgroundDrawable(
                ColorDrawable(
                    ContextCompat.getColor(
                        requireContext(),
                        R.color.fx_mobile_private_layer_color_1,
                    ),
                ),
            )
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()

        onboardingView = null
        homeMenuView = null
        tabCounterView = null
        toolbarView = null
        _interactor = null
        _binding = null
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)

        homeMenuView?.dismissMenu()
    }

    private fun dispatchOnboardingStateChanges(state: OnboardingState) {
        if (state != onboardingStore.state.onboardingState) {
            onboardingStore.dispatch(OnboardingAction.UpdateState(state))
        }
    }
}
