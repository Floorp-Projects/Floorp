/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.content.Context
import android.os.Bundle
import android.os.StrictMode
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.content.ContextCompat
import androidx.lifecycle.Observer
import androidx.navigation.fragment.findNavController
import com.google.android.material.snackbar.Snackbar
import kotlinx.android.synthetic.main.fragment_browser.*
import kotlinx.android.synthetic.main.fragment_browser.view.*
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.session.Session
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.thumbnails.BrowserThumbnails
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.feature.app.links.AppLinksUseCases
import mozilla.components.feature.contextmenu.ContextMenuCandidate
import mozilla.components.feature.readerview.ReaderViewFeature
import mozilla.components.feature.search.SearchFeature
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.tabs.WindowFeature
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.mozilla.fenix.R
import org.mozilla.fenix.addons.runIfFragmentIsAttached
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.navigateSafe
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.resetPoliciesAfter
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.shortcut.PwaOnboardingObserver
import org.mozilla.fenix.trackingprotection.TrackingProtectionOverlay

/**
 * Fragment used for browsing the web within the main app.
 */
@ExperimentalCoroutinesApi
@Suppress("TooManyFunctions", "LargeClass")
class BrowserFragment : BaseBrowserFragment(), UserInteractionHandler {

    private val windowFeature = ViewBoundFeatureWrapper<WindowFeature>()
    private val searchFeature = ViewBoundFeatureWrapper<SearchFeature>()
    private val thumbnailsFeature = ViewBoundFeatureWrapper<BrowserThumbnails>()

    private var readerModeAvailable = false

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        val view = super.onCreateView(inflater, container, savedInstanceState)

        startPostponedEnterTransition()
        return view
    }

    @Suppress("LongMethod")
    override fun initializeUI(view: View): Session? {
        val context = requireContext()
        val components = context.components

        return super.initializeUI(view)?.also {
            gestureLayout.addGestureListener(
                ToolbarGestureHandler(
                    activity = requireActivity(),
                    contentLayout = browserLayout,
                    tabPreview = tabPreview,
                    toolbarLayout = browserToolbarView.view,
                    sessionManager = components.core.sessionManager
                )
            )

            val readerModeAction =
                BrowserToolbar.ToggleButton(
                    image = ContextCompat.getDrawable(requireContext(), R.drawable.ic_readermode)!!,
                    imageSelected =
                        AppCompatResources.getDrawable(requireContext(), R.drawable.ic_readermode_selected)!!,
                    contentDescription = requireContext().getString(R.string.browser_menu_read),
                    contentDescriptionSelected = requireContext().getString(R.string.browser_menu_read_close),
                    visible = {
                        readerModeAvailable
                    },
                    selected = getSessionById()?.let {
                            activity?.components?.core?.store?.state?.findTab(it.id)?.readerState?.active
                        } ?: false,
                    listener = browserInteractor::onReaderModePressed
                )

            browserToolbarView.view.addPageAction(readerModeAction)

            thumbnailsFeature.set(
                feature = BrowserThumbnails(context, view.engineView, components.core.store),
                owner = this,
                view = view
            )

            readerViewFeature.set(
                feature = StrictMode.allowThreadDiskReads().resetPoliciesAfter {
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

                        runIfFragmentIsAttached {
                            browserToolbarView.view.invalidateActions()
                            browserToolbarView.toolbarIntegration.invalidateMenu()
                        }
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
            searchFeature.set(
                feature = SearchFeature(components.core.store) {
                    if (it.isPrivate) {
                        components.useCases.searchUseCases.newPrivateTabSearch.invoke(
                            it.query,
                            parentSession = getSessionById()
                        )
                    } else {
                        components.useCases.searchUseCases.newTabSearch.invoke(
                            it.query,
                            parentSession = getSessionById()
                        )
                    }
                },
                owner = this,
                view = view
            )
        }
    }

    override fun onStart() {
        super.onStart()
        val context = requireContext()
        val settings = context.settings()
        val session = getSessionById()

        val toolbarSessionObserver = TrackingProtectionOverlay(
            context = context,
            settings = settings,
            metrics = context.components.analytics.metrics
        ) {
            browserToolbarView.view
        }
        session?.register(toolbarSessionObserver, viewLifecycleOwner, autoPause = true)

        if (!settings.userKnowsAboutPwas) {
            session?.register(
                PwaOnboardingObserver(
                    navController = findNavController(),
                    settings = settings,
                    webAppUseCases = context.components.useCases.webAppUseCases
                ),
                owner = this,
                autoPause = true
            )
        }

        subscribeToTabCollections()
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

    override fun navToQuickSettingsSheet(session: Session, sitePermissions: SitePermissions?) {
        val directions =
            BrowserFragmentDirections.actionBrowserFragmentToQuickSettingsSheetDialogFragment(
                sessionId = session.id,
                url = session.url,
                title = session.title,
                isSecured = session.securityInfo.secure,
                sitePermissions = sitePermissions,
                gravity = getAppropriateLayoutGravity(),
                certificateName = session.securityInfo.issuer
            )
        nav(R.id.browserFragment, directions)
    }

    override fun navToTrackingProtectionPanel(session: Session) {
        val navController = findNavController()

        requireComponents.useCases.trackingProtectionUseCases.containsException(session.id) { contains ->
            val isEnabled = session.trackerBlockingEnabled && !contains
            val directions =
                BrowserFragmentDirections.actionBrowserFragmentToTrackingProtectionPanelDialogFragment(
                    sessionId = session.id,
                    url = session.url,
                    trackingProtectionEnabled = isEnabled,
                    gravity = getAppropriateLayoutGravity()
                )
            navController.navigateSafe(R.id.browserFragment, directions)
        }
    }

    private val collectionStorageObserver = object : TabCollectionStorage.Observer {
        override fun onCollectionCreated(title: String, sessions: List<Session>) {
            showTabSavedToCollectionSnackbar(sessions.size, true)
        }

        override fun onTabsAdded(tabCollection: TabCollection, sessions: List<Session>) {
            showTabSavedToCollectionSnackbar(sessions.size)
        }

        private fun showTabSavedToCollectionSnackbar(tabSize: Int, isNewCollection: Boolean = false) {
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
        ) + ContextMenuCandidate.createOpenInExternalAppCandidate(requireContext(),
            contextMenuCandidateAppLinksUseCases)
    }
}
