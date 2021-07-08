/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.content.Context
import android.os.StrictMode
import android.view.View
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import androidx.appcompat.content.res.AppCompatResources
import androidx.lifecycle.Observer
import androidx.navigation.fragment.findNavController
import com.google.android.material.snackbar.Snackbar
import kotlinx.android.synthetic.main.fragment_browser.*
import kotlinx.android.synthetic.main.fragment_browser.view.*
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.thumbnails.BrowserThumbnails
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.feature.app.links.AppLinksUseCases
import mozilla.components.feature.contextmenu.ContextMenuCandidate
import mozilla.components.feature.readerview.ReaderViewFeature
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.tabs.WindowFeature
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.navigateSafe
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.shortcut.PwaOnboardingObserver
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.trackingprotection.TrackingProtectionOverlay

/**
 * Fragment used for browsing the web within the main app.
 */
@ExperimentalCoroutinesApi
@Suppress("TooManyFunctions", "LargeClass")
class BrowserFragment : BaseBrowserFragment(), UserInteractionHandler {

    private val windowFeature = ViewBoundFeatureWrapper<WindowFeature>()
    private val openInAppOnboardingObserver = ViewBoundFeatureWrapper<OpenInAppOnboardingObserver>()
    private val trackingProtectionOverlayObserver =
        ViewBoundFeatureWrapper<TrackingProtectionOverlay>()

    private var readerModeAvailable = false
    private var pwaOnboardingObserver: PwaOnboardingObserver? = null

    @Suppress("LongMethod")
    override fun initializeUI(view: View, tab: SessionState) {
        super.initializeUI(view, tab)

        val context = requireContext()
        val components = context.components

        if (context.settings().isSwipeToolbarToSwitchTabsEnabled) {
            gestureLayout.addGestureListener(
                ToolbarGestureHandler(
                    activity = requireActivity(),
                    contentLayout = browserLayout,
                    tabPreview = tabPreview,
                    toolbarLayout = browserToolbarView.view,
                    store = components.core.store,
                    selectTabUseCase = components.useCases.tabsUseCases.selectTab
                )
            )
        }

        if (FeatureFlags.showHomeButtonFeature) {
            val homeAction = BrowserToolbar.Button(
                imageDrawable = AppCompatResources.getDrawable(
                    requireContext(),
                    R.drawable.mozac_ic_home
                )!!,
                contentDescription = requireContext().getString(R.string.browser_toolbar_home),
                iconTintColorResource = ThemeManager.resolveAttribute(R.attr.primaryText, context),
                listener = browserToolbarInteractor::onHomeButtonClicked
            )

            browserToolbarView.view.addNavigationAction(homeAction)
        }

        val readerModeAction =
            BrowserToolbar.ToggleButton(
                image = AppCompatResources.getDrawable(
                    requireContext(),
                    R.drawable.ic_readermode
                )!!,
                imageSelected =
                AppCompatResources.getDrawable(
                    requireContext(),
                    R.drawable.ic_readermode_selected
                )!!,
                contentDescription = requireContext().getString(R.string.browser_menu_read),
                contentDescriptionSelected = requireContext().getString(R.string.browser_menu_read_close),
                visible = {
                    readerModeAvailable
                },
                selected = getCurrentTab()?.let {
                    activity?.components?.core?.store?.state?.findTab(it.id)?.readerState?.active
                } ?: false,
                listener = browserToolbarInteractor::onReaderModePressed
            )

        browserToolbarView.view.addPageAction(readerModeAction)

        thumbnailsFeature.set(
            feature = BrowserThumbnails(context, view.engineView, components.core.store),
            owner = this,
            view = view
        )

        readerViewFeature.set(
            feature = components.strictMode.resetAfter(StrictMode.allowThreadDiskReads()) {
                ReaderViewFeature(
                    context,
                    components.core.engine,
                    components.core.store,
                    view.readerViewControlsBar
                ) { available, active ->
                    if (available) {
                        components.analytics.metrics.track(Event.ReaderModeAvailable)
                    }

                    readerModeAvailable = available
                    readerModeAction.setSelected(active)
                    safeInvalidateBrowserToolbarView()
                }
            },
            owner = this,
            view = view
        )

        windowFeature.set(
            feature = WindowFeature(
                store = components.core.store,
                tabsUseCases = components.useCases.tabsUseCases
            ),
            owner = this,
            view = view
        )

        if (context.settings().shouldShowOpenInAppCfr) {
            openInAppOnboardingObserver.set(
                feature = OpenInAppOnboardingObserver(
                    context = context,
                    store = context.components.core.store,
                    lifecycleOwner = this,
                    navController = findNavController(),
                    settings = context.settings(),
                    appLinksUseCases = context.components.useCases.appLinksUseCases,
                    container = browserLayout as ViewGroup,
                    shouldScrollWithTopToolbar = !context.settings().shouldUseBottomToolbar
                ),
                owner = this,
                view = view
            )
        }
        if (context.settings().shouldShowTrackingProtectionCfr) {
            trackingProtectionOverlayObserver.set(
                feature = TrackingProtectionOverlay(
                    context = context,
                    store = context.components.core.store,
                    lifecycleOwner = viewLifecycleOwner,
                    settings = context.settings(),
                    metrics = context.components.analytics.metrics,
                    getToolbar = { browserToolbarView.view }
                ),
                owner = this,
                view = view
            )
        }
    }

    override fun onStart() {
        super.onStart()
        val context = requireContext()
        val settings = context.settings()

        if (!settings.userKnowsAboutPwas) {
            pwaOnboardingObserver = PwaOnboardingObserver(
                store = context.components.core.store,
                lifecycleOwner = this,
                navController = findNavController(),
                settings = settings,
                webAppUseCases = context.components.useCases.webAppUseCases
            ).also {
                it.start()
            }
        }

        subscribeToTabCollections()
    }

    override fun onStop() {
        super.onStop()
        updateLastBrowseActivity()
        if (FeatureFlags.historyMetadataFeature) {
            updateHistoryMetadata()
        }
        pwaOnboardingObserver?.stop()
    }

    private fun updateHistoryMetadata() {
        getCurrentTab()?.let { tab ->
            (tab as? TabSessionState)?.historyMetadata?.let {
                requireComponents.core.historyMetadataService.updateMetadata(it, tab)
            }
        }
    }

    private fun subscribeToTabCollections() {
        Observer<List<TabCollection>> {
            requireComponents.core.tabCollectionStorage.cachedTabCollections = it
        }.also { observer ->
            requireComponents.core.tabCollectionStorage.getCollections()
                .observe(viewLifecycleOwner, observer)
        }
    }

    override fun onResume() {
        super.onResume()
        requireComponents.core.tabCollectionStorage.register(collectionStorageObserver, this)
    }

    override fun onBackPressed(): Boolean {
        return readerViewFeature.onBackPressed() || super.onBackPressed()
    }

    override fun navToQuickSettingsSheet(tab: SessionState, sitePermissions: SitePermissions?) {
        val directions =
            BrowserFragmentDirections.actionBrowserFragmentToQuickSettingsSheetDialogFragment(
                sessionId = tab.id,
                url = tab.content.url,
                title = tab.content.title,
                isSecured = tab.content.securityInfo.secure,
                sitePermissions = sitePermissions,
                gravity = getAppropriateLayoutGravity(),
                certificateName = tab.content.securityInfo.issuer,
                permissionHighlights = tab.content.permissionHighlights
            )
        nav(R.id.browserFragment, directions)
    }

    override fun navToTrackingProtectionPanel(tab: SessionState) {
        val navController = findNavController()

        requireComponents.useCases.trackingProtectionUseCases.containsException(tab.id) { contains ->
            val isEnabled = tab.trackingProtection.enabled && !contains
            val directions =
                BrowserFragmentDirections.actionBrowserFragmentToTrackingProtectionPanelDialogFragment(
                    sessionId = tab.id,
                    url = tab.content.url,
                    trackingProtectionEnabled = isEnabled,
                    gravity = getAppropriateLayoutGravity()
                )
            navController.navigateSafe(R.id.browserFragment, directions)
        }
    }

    private val collectionStorageObserver = object : TabCollectionStorage.Observer {
        override fun onCollectionCreated(
            title: String,
            sessions: List<TabSessionState>,
            id: Long?
        ) {
            showTabSavedToCollectionSnackbar(sessions.size, true)
        }

        override fun onTabsAdded(tabCollection: TabCollection, sessions: List<TabSessionState>) {
            showTabSavedToCollectionSnackbar(sessions.size)
        }

        private fun showTabSavedToCollectionSnackbar(
            tabSize: Int,
            isNewCollection: Boolean = false
        ) {
            view?.let { view ->
                val messageStringRes = when {
                    isNewCollection -> {
                        R.string.create_collection_tabs_saved_new_collection
                    }
                    tabSize > 1 -> {
                        R.string.create_collection_tabs_saved
                    }
                    else -> {
                        R.string.create_collection_tab_saved
                    }
                }
                FenixSnackbar.make(
                    view = view.browserLayout,
                    duration = Snackbar.LENGTH_SHORT,
                    isDisplayedWithBrowserToolbar = true
                )
                    .setText(view.context.getString(messageStringRes))
                    .setAction(requireContext().getString(R.string.create_collection_view)) {
                        findNavController().navigate(
                            BrowserFragmentDirections.actionGlobalHome(focusOnAddressBar = false)
                        )
                    }
                    .show()
            }
        }
    }

    override fun getContextMenuCandidates(
        context: Context,
        view: View
    ): List<ContextMenuCandidate> {
        val contextMenuCandidateAppLinksUseCases = AppLinksUseCases(
            requireContext(),
            { true }
        )

        return ContextMenuCandidate.defaultCandidates(
            context,
            context.components.useCases.tabsUseCases,
            context.components.useCases.contextMenuUseCases,
            view,
            FenixSnackbarDelegate(view)
        ) + ContextMenuCandidate.createOpenInExternalAppCandidate(
            requireContext(),
            contextMenuCandidateAppLinksUseCases
        )
    }

    /**
     * Updates the last time the user was active on the [BrowserFragment].
     * This is useful to determine if the user has to start on the [HomeFragment]
     * or it should go directly to the [BrowserFragment].
     */
    @VisibleForTesting
    internal fun updateLastBrowseActivity() {
        requireContext().settings().lastBrowseActivity = System.currentTimeMillis()
    }
}
