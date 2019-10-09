/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

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
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.transition.TransitionInflater
import com.google.android.material.snackbar.Snackbar
import kotlinx.android.synthetic.main.fragment_browser.view.*
import kotlinx.android.synthetic.main.tracking_protection_onboarding_popup.view.*
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.session.Session
import mozilla.components.feature.readerview.ReaderViewFeature
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.jetbrains.anko.dimen
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.readermode.DefaultReaderModeController
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.toolbar.BrowserInteractor
import org.mozilla.fenix.components.toolbar.BrowserToolbarController
import org.mozilla.fenix.components.toolbar.BrowserToolbarViewInteractor
import org.mozilla.fenix.components.toolbar.QuickActionSheetAction
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.home.sessioncontrol.SessionControlChange
import org.mozilla.fenix.home.sessioncontrol.TabCollection
import org.mozilla.fenix.mvi.getManagedEmitter
import org.mozilla.fenix.quickactionsheet.DefaultQuickActionSheetController
import org.mozilla.fenix.quickactionsheet.QuickActionSheetSessionObserver
import org.mozilla.fenix.quickactionsheet.QuickActionSheetView

/**
 * Fragment used for browsing the web within the main app.
 */
@ExperimentalCoroutinesApi
@Suppress("TooManyFunctions", "LargeClass")
class BrowserFragment : BaseBrowserFragment(), BackHandler {
    private lateinit var quickActionSheetView: QuickActionSheetView
    private var quickActionSheetSessionObserver: QuickActionSheetSessionObserver? = null

    private val readerViewFeature = ViewBoundFeatureWrapper<ReaderViewFeature>()

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

                    browserStore.apply {
                        dispatch(QuickActionSheetAction.ReadableStateChange(available))
                        dispatch(
                            QuickActionSheetAction.ReaderActiveStateChange(
                                sessionManager.selectedSession?.readerMode ?: false
                            )
                        )
                    }
                },
                owner = this,
                view = view
            )

            if ((activity as HomeActivity).browsingModeManager.mode.isPrivate) {
                // We need to update styles for private mode programmatically for now:
                // https://github.com/mozilla-mobile/android-components/issues/3400
                themeReaderViewControlsForPrivateMode(view.readerViewControlsBar)
            }

            consumeFrom(browserStore) {
                quickActionSheetView.update(it)
                browserToolbarView.update(it)
            }
        }
    }

    override fun onStart() {
        super.onStart()
        subscribeToTabCollections()
        quickActionSheetSessionObserver = QuickActionSheetSessionObserver(
            lifecycleScope,
            requireComponents,
            dispatch = { action -> browserStore.dispatch(action) }
        ).also { observer ->
            getSessionById()?.register(observer, this, autoPause = true)
        }
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
            quickActionSheetSessionObserver?.updateBookmarkState(it)
        }
        requireComponents.core.tabCollectionStorage.register(collectionStorageObserver, this)
    }

    override fun onBackPressed(): Boolean {
        return readerViewFeature.onBackPressed() || super.onBackPressed()
    }

    override fun createBrowserToolbarViewInteractor(
        browserToolbarController: BrowserToolbarController,
        session: Session?
    ): BrowserToolbarViewInteractor {
        val context = requireContext()

        val interactor = BrowserInteractor(
            context = context,
            store = browserStore,
            browserToolbarController = browserToolbarController,
            quickActionSheetController = DefaultQuickActionSheetController(
                context = context,
                navController = findNavController(),
                sessionManager = context.components.core.sessionManager,
                appLinksUseCases = context.components.useCases.appLinksUseCases,
                bookmarkTapped = {
                    lifecycleScope.launch { bookmarkTapped(it) }
                }
            ),
            readerModeController = DefaultReaderModeController(readerViewFeature),
            currentSession = session
        )

        quickActionSheetView = QuickActionSheetView(view!!.nestedScrollQuickAction, interactor)

        return interactor
    }

    override fun navToQuickSettingsSheet(session: Session, sitePermissions: SitePermissions?) {
        val directions =
            BrowserFragmentDirections.actionBrowserFragmentToQuickSettingsSheetDialogFragment(
                sessionId = session.id,
                url = session.url,
                isSecured = session.securityInfo.secure,
                isTrackingProtectionOn = session.trackerBlockingEnabled,
                sitePermissions = sitePermissions,
                gravity = getAppropriateLayoutGravity()
            )
        nav(R.id.browserFragment, directions)
    }

    override fun navToTrackingProtectionPanel(session: Session) {
        val directions =
            BrowserFragmentDirections.actionBrowserFragmentToTrackingProtectionPanelDialogFragment(
                sessionId = session.id,
                url = session.url,
                trackingProtectionEnabled = session.trackerBlockingEnabled,
                gravity = getAppropriateLayoutGravity()
            )
        nav(R.id.browserFragment, directions)
    }

    override fun getEngineMargins(): Pair<Int, Int> {
        val toolbarAndQASSize = resources.getDimensionPixelSize(R.dimen.toolbar_and_qab_height)
        return 0 to toolbarAndQASSize
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

    private suspend fun bookmarkTapped(session: Session) = withContext(IO) {
        val bookmarksStorage = requireComponents.core.bookmarksStorage
        val existing =
            bookmarksStorage.getBookmarksWithUrl(session.url).firstOrNull { it.url == session.url }
        if (existing != null) {
            // Bookmark exists, go to edit fragment
            withContext(Main) {
                nav(
                    R.id.browserFragment,
                    BrowserFragmentDirections.actionBrowserFragmentToBookmarkEditFragment(existing.guid)
                )
            }
        } else {
            // Save bookmark, then go to edit fragment
            val guid = bookmarksStorage.addItem(
                BookmarkRoot.Mobile.id,
                url = session.url,
                title = session.title,
                position = null
            )

            withContext(Main) {
                browserStore.dispatch(
                    QuickActionSheetAction.BookmarkedStateChange(bookmarked = true)
                )
                requireComponents.analytics.metrics.track(Event.AddBookmark)

                view?.let {
                    FenixSnackbar.make(it.rootView, Snackbar.LENGTH_LONG)
                        .setAnchorView(browserToolbarView.view)
                        .setAction(getString(R.string.edit_bookmark_snackbar_action)) {
                            nav(
                                R.id.browserFragment,
                                BrowserFragmentDirections.actionBrowserFragmentToBookmarkEditFragment(
                                    guid
                                )
                            )
                        }
                        .setText(getString(R.string.bookmark_saved_snackbar))
                        .show()
                }
            }
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

    override fun onSessionSelected(session: Session) {
        super.onSessionSelected(session)
        quickActionSheetSessionObserver?.updateBookmarkState(session)
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
        if (!FeatureFlags.etpCategories) {
            return
        }
        context?.let {
            val layout = LayoutInflater.from(it)
                .inflate(R.layout.tracking_protection_onboarding_popup, null)
            layout.onboarding_message.text =
                it.getString(R.string.etp_onboarding_message, getString(R.string.app_name))

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
                    .findViewById<AppCompatImageView>(R.id.mozac_browser_toolbar_tracking_protection_icon_view)

            // Measure layout view
            val spec = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED)
            layout.measure(spec, spec)

            val containerHeight = layout.measuredHeight

            val xOffset = it.dimen(R.dimen.tp_onboarding_x_offset)

            // Positioning the popup above the tp anchor.
            val yOffset = -containerHeight - (browserToolbarView.view.height / THREE * 2)

            trackingOnboarding.showAsDropDown(tpIcon, xOffset, yOffset)
            it.settings().incrementTrackingProtectionOnboardingCount()
        }
    }

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
