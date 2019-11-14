/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.content.Context
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.os.Bundle
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.Button
import android.widget.ImageView
import android.widget.PopupWindow
import android.widget.RadioButton
import androidx.appcompat.widget.AppCompatImageView
import androidx.core.content.ContextCompat
import androidx.lifecycle.Observer
import androidx.transition.TransitionInflater
import com.google.android.material.snackbar.Snackbar
import kotlinx.android.synthetic.main.fragment_browser.view.*
import kotlinx.android.synthetic.main.fragment_home.*
import kotlinx.android.synthetic.main.tracking_protection_onboarding_popup.view.*
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.session.Session
import mozilla.components.feature.contextmenu.ContextMenuCandidate
import mozilla.components.feature.readerview.ReaderViewFeature
import mozilla.components.feature.session.TrackingProtectionUseCases
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.feature.tabs.WindowFeature
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.jetbrains.anko.dimen
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getDimenInDip
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.home.sessioncontrol.SessionControlChange
import org.mozilla.fenix.home.sessioncontrol.TabCollection
import org.mozilla.fenix.mvi.getManagedEmitter

/**
 * Fragment used for browsing the web within the main app.
 */
@ExperimentalCoroutinesApi
@Suppress("TooManyFunctions")
class BrowserFragment : BaseBrowserFragment(), BackHandler {

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
        subscribeToTabCollections()
        getSessionById()?.register(toolbarSessionObserver, this, autoPause = true)
    }

    private val toolbarSessionObserver = object : Session.Observer {
        override fun onLoadingStateChanged(session: Session, loading: Boolean) {
            if (!loading &&
                shouldShowTrackingProtectionOnboarding(session)
            ) {
                showTrackingProtectionOnboarding()
            }
        }
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

    private fun subscribeToTabCollections() {
        requireComponents.core.tabCollectionStorage.getCollections().observe(this, Observer {
            requireComponents.core.tabCollectionStorage.cachedTabCollections = it
            getManagedEmitter<SessionControlChange>().onNext(
                SessionControlChange.CollectionsChange(
                    it
                )
            )
        })
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
                    .setAnchorView(browserToolbarView.view)
                    .show()
            }
        }
    }

    private fun showTrackingProtectionOnboarding() {
        context?.let {
            val layout = LayoutInflater.from(it)
                .inflate(R.layout.tracking_protection_onboarding_popup, null)
            layout.onboarding_message.text =
                it.getString(R.string.etp_onboarding_message_2, getString(R.string.app_name))

            val trackingOnboarding = PopupWindow(
                layout,
                it.dimen(R.dimen.tp_onboarding_width),
                WindowManager.LayoutParams.WRAP_CONTENT
            ).apply {
                setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
                isOutsideTouchable = true
                isFocusable = true
                elevation = view!!.resources.getDimension(R.dimen.mozac_browser_menu_elevation)
                animationStyle = R.style.Mozac_Browser_Menu_Animation_OverflowMenuBottom
            }

            val closeButton = layout.findViewById<ImageView>(R.id.close_onboarding)
            closeButton.increaseTapArea(BUTTON_INCREASE_DPS)
            closeButton.setOnClickListener {
                trackingOnboarding.dismiss()
            }

            val tpIcon =
                browserToolbarView
                    .view
                    .findViewById<AppCompatImageView>(R.id.mozac_browser_toolbar_tracking_protection_indicator)

            // Measure layout view
            val spec = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED)
            layout.measure(spec, spec)

            val containerHeight = layout.measuredHeight
            val triangleHeight = it.getDimenInDip(R.dimen.tp_onboarding_triangle_height).toInt()

            val xOffset = it.dimen(R.dimen.tp_onboarding_x_offset)

            // Positioning the popup above the tp anchor.
            val yOffset =
                -containerHeight - (browserToolbarView.view.height / THREE * 2) + triangleHeight

            trackingOnboarding.showAsDropDown(tpIcon, xOffset, yOffset)
            it.settings().incrementTrackingProtectionOnboardingCount()
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
            bottom_bar
        )
    )

    private fun shouldShowTrackingProtectionOnboarding(session: Session) =
        context?.settings()?.shouldShowTrackingProtectionOnboarding ?: false &&
                session.trackerBlockingEnabled && session.trackersBlocked.isNotEmpty()

    companion object {
        private const val THREE = 3
        private const val BUTTON_INCREASE_DPS = 12
        private const val SHARED_TRANSITION_MS = 200L
        private const val TAB_ITEM_TRANSITION_NAME = "tab_item"
        const val REPORT_SITE_ISSUE_URL =
            "https://webcompat.com/issues/new?url=%s&label=browser-fenix"
    }
}
