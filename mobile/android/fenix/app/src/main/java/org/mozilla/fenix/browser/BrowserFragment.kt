/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.content.Context
import android.os.Bundle
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.RadioButton
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.content.ContextCompat
import androidx.transition.TransitionInflater
import com.google.android.material.snackbar.Snackbar
import kotlinx.android.synthetic.main.fragment_browser.*
import kotlinx.android.synthetic.main.fragment_browser.view.*
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.session.Session
import mozilla.components.feature.contextmenu.ContextMenuCandidate
import mozilla.components.feature.readerview.ReaderViewFeature
import mozilla.components.feature.session.TrackingProtectionUseCases
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.tabs.WindowFeature
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.jetbrains.anko.dimen
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.trackingprotection.TrackingProtectionOverlay

/**
 * Fragment used for browsing the web within the main app.
 */
@ExperimentalCoroutinesApi
@Suppress("TooManyFunctions", "LargeClass")
class BrowserFragment : BaseBrowserFragment(), UserInteractionHandler {

    private val windowFeature = ViewBoundFeatureWrapper<WindowFeature>()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        postponeEnterTransition()
        sharedElementEnterTransition =
            TransitionInflater.from(context).inflateTransition(android.R.transition.move)
                .setDuration(
                    SHARED_TRANSITION_MS
                )
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        val view = super.onCreateView(inflater, container, savedInstanceState)
        view.browserLayout.transitionName = "$TAB_ITEM_TRANSITION_NAME${getSessionById()?.id}"

        startPostponedEnterTransition()

        return view
    }

    override fun initializeUI(view: View): Session? {
        val context = requireContext()
        val sessionManager = context.components.core.sessionManager

        return super.initializeUI(view)?.also {
            readerViewFeature.set(
                feature = ReaderViewFeature(
                    context,
                    context.components.core.engine,
                    sessionManager,
                    view.readerViewControlsBar
                ) { available ->
                    if (available) {
                        context.components.analytics.metrics.track(Event.ReaderModeAvailable)
                    }
                },
                owner = this,
                view = view
            )

            windowFeature.set(
                feature = WindowFeature(
                    store = context.components.core.store,
                    tabsUseCases = context.components.useCases.tabsUseCases
                ),
                owner = this,
                view = view
            )

            if ((activity as HomeActivity).browsingModeManager.mode.isPrivate) {
                // We need to update styles for private mode programmatically for now:
                // https://github.com/mozilla-mobile/android-components/issues/3400
                themeReaderViewControlsForPrivateMode(view.readerViewControlsBar)
            }

            consumeFrom(browserFragmentStore) {
                browserToolbarView.update(it)
            }
        }
    }

    override fun onStart() {
        super.onStart()
        val toolbarSessionObserver = TrackingProtectionOverlay(
            context = requireContext(),
            settings = requireContext().settings()
        ) {
            browserToolbarView.view
        }
        getSessionById()?.register(toolbarSessionObserver, this, autoPause = true)
        updateEngineBottomMargin()
    }

    private fun updateEngineBottomMargin() {
        val browserEngine = swipeRefresh.layoutParams as CoordinatorLayout.LayoutParams

        browserEngine.bottomMargin = if (requireContext().settings().shouldUseBottomToolbar) {
            requireContext().dimen(R.dimen.browser_toolbar_height)
        } else {
            0
        }

        val toolbarSessionObserver = TrackingProtectionOverlay(
            context = requireContext(),
            settings = requireContext().settings()
        ) {
            browserToolbarView.view
        }
        getSessionById()?.register(toolbarSessionObserver, this, autoPause = true)
    }

    override fun onResume() {
        super.onResume()
        getSessionById()?.let {
            /**
             * The session mode may be changed if the user is originally in Normal Mode and then
             * opens a 3rd party link in Private Browsing Mode. Hence, we update the theme here.
             * This fixes issue #5254.
             */
            (activity as HomeActivity).updateThemeForSession(it)
        }
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
                isSecured = session.securityInfo.secure,
                sitePermissions = sitePermissions,
                gravity = getAppropriateLayoutGravity()
            )
        nav(R.id.browserFragment, directions)
    }

    override fun navToTrackingProtectionPanel(session: Session) {
        val useCase = TrackingProtectionUseCases(
            sessionManager = requireComponents.core.sessionManager,
            engine = requireComponents.core.engine
        )
        useCase.containsException(session) { contains ->
            val isEnabled = session.trackerBlockingEnabled && !contains
            val directions =
                BrowserFragmentDirections.actionBrowserFragmentToTrackingProtectionPanelDialogFragment(
                    sessionId = session.id,
                    url = session.url,
                    trackingProtectionEnabled = isEnabled,
                    gravity = getAppropriateLayoutGravity()
                )
            nav(R.id.browserFragment, directions)
        }
    }

    override fun getEngineMargins(): Pair<Int, Int> {
        val toolbarSize = resources.getDimensionPixelSize(R.dimen.browser_toolbar_height)
        return 0 to toolbarSize
    }

    override fun getAppropriateLayoutGravity() = Gravity.BOTTOM

    private fun themeReaderViewControlsForPrivateMode(view: View) = with(view) {
        listOf(
            R.id.mozac_feature_readerview_font_size_decrease,
            R.id.mozac_feature_readerview_font_size_increase
        ).map {
            findViewById<Button>(it)
        }.forEach {
            it.setTextColor(
                ContextCompat.getColorStateList(
                    context,
                    R.color.readerview_private_button_color
                )
            )
        }

        listOf(
            R.id.mozac_feature_readerview_font_serif,
            R.id.mozac_feature_readerview_font_sans_serif
        ).map {
            findViewById<RadioButton>(it)
        }.forEach {
            it.setTextColor(
                ContextCompat.getColorStateList(
                    context,
                    R.color.readerview_private_radio_color
                )
            )
        }
    }

    private val collectionStorageObserver = object : TabCollectionStorage.Observer {
        override fun onCollectionCreated(title: String, sessions: List<Session>) {
            showTabSavedToCollectionSnackbar()
        }

        override fun onTabsAdded(tabCollection: TabCollection, sessions: List<Session>) {
            showTabSavedToCollectionSnackbar()
        }

        private fun showTabSavedToCollectionSnackbar() {
            view?.let { view ->
                FenixSnackbar.make(view, Snackbar.LENGTH_SHORT)
                    .setText(view.context.getString(R.string.create_collection_tab_saved))
                    .setAnchorView(browserToolbarView.getSnackbarAnchor())
                    .show()
            }
        }
    }

    override fun getContextMenuCandidates(
        context: Context,
        view: View
    ): List<ContextMenuCandidate> = ContextMenuCandidate.defaultCandidates(
        context,
        context.components.useCases.tabsUseCases,
        context.components.useCases.contextMenuUseCases,
        view,
        FenixSnackbarDelegate(
            view,
            browserToolbarView.view
        )
    )

    companion object {
        private const val SHARED_TRANSITION_MS = 200L
        private const val TAB_ITEM_TRANSITION_NAME = "tab_item"
        const val REPORT_SITE_ISSUE_URL =
            "https://webcompat.com/issues/new?url=%s&label=browser-fenix"
    }
}
