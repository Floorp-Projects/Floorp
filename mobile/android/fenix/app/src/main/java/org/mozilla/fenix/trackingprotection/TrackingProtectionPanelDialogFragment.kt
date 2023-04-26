/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import android.app.Dialog
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.os.Bundle
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.LinearLayout
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.appcompat.view.ContextThemeWrapper
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.withStarted
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialog
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.mapNotNull
import kotlinx.coroutines.launch
import kotlinx.coroutines.plus
import mozilla.components.browser.state.selector.findTabOrCustomTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.session.TrackingProtectionUseCases
import mozilla.components.lib.state.ext.consumeFlow
import mozilla.components.lib.state.ext.observe
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.GleanMetrics.TrackingProtection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.databinding.FragmentTrackingProtectionBinding
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.settings.SupportUtils

@Suppress("TooManyFunctions")
class TrackingProtectionPanelDialogFragment : AppCompatDialogFragment(), UserInteractionHandler {

    private val args by navArgs<TrackingProtectionPanelDialogFragmentArgs>()

    private fun inflateRootView(container: ViewGroup? = null): View {
        val contextThemeWrapper = ContextThemeWrapper(
            activity,
            (activity as HomeActivity).themeManager.currentThemeResource,
        )
        return LayoutInflater.from(contextThemeWrapper).inflate(
            R.layout.fragment_tracking_protection,
            container,
            false,
        )
    }

    @VisibleForTesting
    internal lateinit var protectionsStore: ProtectionsStore
    private lateinit var trackingProtectionView: TrackingProtectionPanelView
    private lateinit var trackingProtectionInteractor: TrackingProtectionPanelInteractor
    private lateinit var trackingProtectionUseCases: TrackingProtectionUseCases

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        trackingProtectionUseCases = requireComponents.useCases.trackingProtectionUseCases
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        val store = requireComponents.core.store
        val view = inflateRootView(container)
        val tab = store.state.findTabOrCustomTab(provideCurrentTabId())

        protectionsStore = StoreProvider.get(this) {
            ProtectionsStore(
                ProtectionsState(
                    tab = tab,
                    url = args.url,
                    isTrackingProtectionEnabled = args.trackingProtectionEnabled,
                    cookieBannerUIMode = args.cookieBannerUIMode,
                    listTrackers = listOf(),
                    mode = ProtectionsState.Mode.Normal,
                    lastAccessedCategory = "",
                ),
            )
        }
        trackingProtectionInteractor = TrackingProtectionPanelInteractor(
            context = requireContext(),
            fragment = this,
            store = protectionsStore,
            ioScope = viewLifecycleOwner.lifecycleScope + Dispatchers.IO,
            cookieBannersStorage = requireComponents.core.cookieBannersStorage,
            navController = { findNavController() },
            openTrackingProtectionSettings = ::openTrackingProtectionSettings,
            openLearnMoreLink = ::handleLearnMoreClicked,
            sitePermissions = args.sitePermissions,
            gravity = args.gravity,
            getCurrentTab = ::getCurrentTab,
        )
        val binding = FragmentTrackingProtectionBinding.bind(view)
        trackingProtectionView =
            TrackingProtectionPanelView(binding.fragmentTp, trackingProtectionInteractor)
        tab?.let { updateTrackers(it) }
        return view
    }

    @VisibleForTesting
    internal fun updateTrackers(tab: SessionState) {
        trackingProtectionUseCases.fetchTrackingLogs(
            tab.id,
            onSuccess = {
                protectionsStore.dispatch(ProtectionsAction.TrackerLogChange(it))
            },
            onError = {
                Logger.error("TrackingProtectionUseCases - fetchTrackingLogs onError", it)
            },
        )
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val store = requireComponents.core.store

        observeUrlChange(store)
        observeTrackersChange(store)
        protectionsStore.observe(view) {
            viewLifecycleOwner.lifecycleScope.launch {
                withStarted {
                    trackingProtectionView.update(it)
                }
            }
        }
    }

    private fun openTrackingProtectionSettings() {
        TrackingProtection.panelSettings.record(NoExtras())
        nav(
            R.id.trackingProtectionPanelDialogFragment,
            TrackingProtectionPanelDialogFragmentDirections.actionGlobalTrackingProtectionFragment(),
        )
    }

    private fun handleLearnMoreClicked() {
        (activity as HomeActivity).openToBrowserAndLoad(
            searchTermOrURL = SupportUtils.getGenericSumoURLForTopic(
                SupportUtils.SumoTopic.SMARTBLOCK,
            ),
            newTab = true,
            from = BrowserDirection.FromTrackingProtectionDialog,
        )
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        return if (args.gravity == Gravity.BOTTOM) {
            object : BottomSheetDialog(requireContext(), this.theme) {
                @Deprecated("Deprecated in Java")
                override fun onBackPressed() {
                    this@TrackingProtectionPanelDialogFragment.onBackPressed()
                }
            }.apply {
                setOnShowListener {
                    val bottomSheet =
                        findViewById<View>(com.google.android.material.R.id.design_bottom_sheet) as FrameLayout
                    val behavior = BottomSheetBehavior.from(bottomSheet)
                    behavior.state = BottomSheetBehavior.STATE_EXPANDED
                }
            }
        } else {
            object : Dialog(requireContext()) {
                @Deprecated("Deprecated in Java")
                override fun onBackPressed() {
                    this@TrackingProtectionPanelDialogFragment.onBackPressed()
                }
            }.applyCustomizationsForTopDialog(inflateRootView())
        }
    }

    private fun Dialog.applyCustomizationsForTopDialog(rootView: View): Dialog {
        addContentView(
            rootView,
            LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.MATCH_PARENT,
            ),
        )

        window?.apply {
            setGravity(args.gravity)
            setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
            // This must be called after addContentView, or it won't fully fill to the edge.
            setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
        }
        return this
    }

    override fun onBackPressed(): Boolean {
        if (!trackingProtectionView.onBackPressed()) {
            dismiss()
        }
        return true
    }

    @VisibleForTesting
    internal fun observeUrlChange(store: BrowserStore) {
        consumeFlow(store) { flow ->
            flow.mapNotNull { state ->
                state.findTabOrCustomTab(provideCurrentTabId())
            }.ifChanged { tab -> tab.content.url }
                .collect {
                    protectionsStore.dispatch(ProtectionsAction.UrlChange(it.content.url))
                }
        }
    }

    @VisibleForTesting
    internal fun provideCurrentTabId(): String = args.sessionId

    @VisibleForTesting
    internal fun observeTrackersChange(store: BrowserStore) {
        consumeFlow(store) { flow ->
            flow.mapNotNull { state ->
                state.findTabOrCustomTab(provideCurrentTabId())
            }.ifAnyChanged { tab ->
                arrayOf(
                    tab.trackingProtection.blockedTrackers,
                    tab.trackingProtection.loadedTrackers,
                )
            }.collect {
                updateTrackers(it)
            }
        }
    }

    private fun getCurrentTab(): SessionState? {
        return requireComponents.core.store.state.findTabOrCustomTab(args.sessionId)
    }
}
