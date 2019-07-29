/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.content.Intent
import android.content.pm.ActivityInfo
import android.os.Bundle
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.RadioButton
import androidx.appcompat.app.AppCompatActivity
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import androidx.lifecycle.Observer
import androidx.lifecycle.ViewModelProviders
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.NavHostFragment.findNavController
import androidx.navigation.fragment.findNavController
import com.google.android.material.snackbar.Snackbar
import kotlinx.android.synthetic.main.component_search.*
import kotlinx.android.synthetic.main.fragment_browser.*
import kotlinx.android.synthetic.main.fragment_browser.view.*
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withContext
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.app.links.AppLinksFeature
import mozilla.components.feature.contextmenu.ContextMenuFeature
import mozilla.components.feature.downloads.DownloadsFeature
import mozilla.components.feature.intent.IntentProcessor
import mozilla.components.feature.prompts.PromptFeature
import mozilla.components.feature.readerview.ReaderViewFeature
import mozilla.components.feature.session.FullScreenFeature
import mozilla.components.feature.session.SessionFeature
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.session.SwipeRefreshFeature
import mozilla.components.feature.session.ThumbnailsFeature
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.feature.sitepermissions.SitePermissionsFeature
import mozilla.components.feature.sitepermissions.SitePermissionsRules
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.ktx.android.view.exitImmersiveModeIfNeeded
import mozilla.components.support.ktx.kotlin.toUri
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ThemeManager
import org.mozilla.fenix.browser.readermode.DefaultReaderModeController
import org.mozilla.fenix.collections.CreateCollectionViewModel
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.FindInPageIntegration
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.toolbar.BrowserInteractor
import org.mozilla.fenix.components.toolbar.BrowserState
import org.mozilla.fenix.components.toolbar.BrowserStore
import org.mozilla.fenix.components.toolbar.BrowserToolbarView
import org.mozilla.fenix.components.toolbar.DefaultBrowserToolbarController
import org.mozilla.fenix.components.toolbar.QuickActionSheetAction
import org.mozilla.fenix.components.toolbar.QuickActionSheetState
import org.mozilla.fenix.components.toolbar.ToolbarIntegration
import org.mozilla.fenix.customtabs.CustomTabsIntegration
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.enterToImmersiveMode
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.home.sessioncontrol.SessionControlChange
import org.mozilla.fenix.mvi.getManagedEmitter
import org.mozilla.fenix.quickactionsheet.DefaultQuickActionSheetController
import org.mozilla.fenix.quickactionsheet.QuickActionSheetView
import org.mozilla.fenix.utils.Settings
import java.net.MalformedURLException
import java.net.URL

@SuppressWarnings("TooManyFunctions", "LargeClass")
class BrowserFragment : Fragment(), BackHandler {
    private lateinit var browserStore: BrowserStore
    private lateinit var browserInteractor: BrowserInteractor

    private lateinit var browserToolbarView: BrowserToolbarView
    private lateinit var quickActionSheetView: QuickActionSheetView

    private var tabCollectionObserver: Observer<List<TabCollection>>? = null
    private var sessionObserver: Session.Observer? = null
    private var sessionManagerObserver: SessionManager.Observer? = null

    private val sessionFeature = ViewBoundFeatureWrapper<SessionFeature>()
    private val contextMenuFeature = ViewBoundFeatureWrapper<ContextMenuFeature>()
    private val downloadsFeature = ViewBoundFeatureWrapper<DownloadsFeature>()
    private val appLinksFeature = ViewBoundFeatureWrapper<AppLinksFeature>()
    private val promptsFeature = ViewBoundFeatureWrapper<PromptFeature>()
    private val findInPageIntegration = ViewBoundFeatureWrapper<FindInPageIntegration>()
    private val toolbarIntegration = ViewBoundFeatureWrapper<ToolbarIntegration>()
    private val readerViewFeature = ViewBoundFeatureWrapper<ReaderViewFeature>()
    private val sitePermissionsFeature = ViewBoundFeatureWrapper<SitePermissionsFeature>()
    private val fullScreenFeature = ViewBoundFeatureWrapper<FullScreenFeature>()
    private val thumbnailsFeature = ViewBoundFeatureWrapper<ThumbnailsFeature>()
    private val swipeRefreshFeature = ViewBoundFeatureWrapper<SwipeRefreshFeature>()
    private val customTabsIntegration = ViewBoundFeatureWrapper<CustomTabsIntegration>()
    private var findBookmarkJob: Job? = null

    var customTabSessionId: String? = null

    /*
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // Disabled while awaiting a better solution to #3209
        postponeEnterTransition()
        sharedElementEnterTransition =
            TransitionInflater.from(context).inflateTransition(android.R.transition.move).setDuration(
                SHARED_TRANSITION_MS
            )
    }
    */

    @SuppressWarnings("ComplexMethod")
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        require(arguments != null)
        customTabSessionId = arguments?.getString(IntentProcessor.ACTIVE_SESSION_ID)

        val view = inflater.inflate(R.layout.fragment_browser, container, false)
        view.browserLayout.transitionName = "$TAB_ITEM_TRANSITION_NAME${getSessionById()?.id}"

        startPostponedEnterTransition()

        val activity = activity as HomeActivity
        ThemeManager.applyStatusBarTheme(activity.window, activity.themeManager, activity)

        val appLink = requireComponents.useCases.appLinksUseCases.appLinkRedirect
        browserStore = StoreProvider.get(this) {
            BrowserStore(
                BrowserState(
                    quickActionSheetState = QuickActionSheetState(
                        readable = getSessionById()?.readerable ?: false,
                        bookmarked = findBookmarkedURL(getSessionById()),
                        readerActive = getSessionById()?.readerMode ?: false,
                        bounceNeeded = false,
                        isAppLink = getSessionById()?.let { appLink.invoke(it.url).hasExternalApp() } ?: false
                    )
                )
            )
        }

        return view
    }

    @Suppress("LongMethod", "ComplexMethod")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val sessionManager = requireComponents.core.sessionManager

        val viewModel = activity!!.run {
            ViewModelProviders.of(this).get(CreateCollectionViewModel::class.java)
        }

        browserInteractor = BrowserInteractor(
            context = context!!,
            store = browserStore,
            browserToolbarController = DefaultBrowserToolbarController(
                context!!,
                findNavController(),
                findInPageLauncher = { findInPageIntegration.withFeature { it.launch() } },
                nestedScrollQuickActionView = nestedScrollQuickAction,
                engineView = engineView,
                currentSession = getSessionById() ?: requireComponents.core.sessionManager.selectedSessionOrThrow,
                viewModel = viewModel
            ),
            quickActionSheetController = DefaultQuickActionSheetController(
                context = context!!,
                navController = findNavController(),
                currentSession = getSessionById() ?: requireComponents.core.sessionManager.selectedSessionOrThrow,
                appLinksUseCases = requireComponents.useCases.appLinksUseCases,
                bookmarkTapped = {
                    lifecycleScope.launch { bookmarkTapped(it) }
                }
            ),
            readerModeController = DefaultReaderModeController(readerViewFeature),
            currentSession = getSessionById() ?: requireComponents.core.sessionManager.selectedSessionOrThrow
        )

        browserToolbarView = BrowserToolbarView(
            container = view.browserLayout,
            interactor = browserInteractor,
            currentSession = getSessionById() ?: requireComponents.core.sessionManager.selectedSessionOrThrow
        )

        toolbarIntegration.set(
            feature = browserToolbarView.toolbarIntegration,
            owner = this,
            view = view
        )

        findInPageIntegration.set(
            feature = FindInPageIntegration(
                requireComponents.core.sessionManager, customTabSessionId, view.findInPageView, view.engineView, toolbar
            ),
            owner = this,
            view = view
        )

        quickActionSheetView = QuickActionSheetView(view.nestedScrollQuickAction, browserInteractor)

        browserToolbarView.view.setOnSiteSecurityClickedListener {
            showQuickSettingsDialog()
        }

        contextMenuFeature.set(
            feature = ContextMenuFeature(
                requireFragmentManager(),
                sessionManager,
                FenixContextMenuCandidate.defaultCandidates(
                    requireContext(),
                    requireComponents.useCases.tabsUseCases,
                    view,
                    FenixSnackbarDelegate(
                        view,
                        if (getSessionById()?.isCustomTabSession() == true) null else nestedScrollQuickAction
                    )
                ),
                view.engineView
            ),
            owner = this,
            view = view
        )

        downloadsFeature.set(
            feature = DownloadsFeature(
                requireContext(),
                sessionManager = sessionManager,
                fragmentManager = childFragmentManager,
                sessionId = customTabSessionId,
                onNeedToRequestPermissions = { permissions ->
                    requestPermissions(permissions, REQUEST_CODE_DOWNLOAD_PERMISSIONS)
                }),
            owner = this,
            view = view
        )

        appLinksFeature.set(
            feature = AppLinksFeature(
                requireContext(),
                sessionManager = sessionManager,
                sessionId = customTabSessionId,
                interceptLinkClicks = true,
                fragmentManager = requireFragmentManager()
            ),
            owner = this,
            view = view
        )

        promptsFeature.set(
            feature = PromptFeature(
                fragment = this,
                sessionManager = sessionManager,
                sessionId = customTabSessionId,
                fragmentManager = requireFragmentManager(),
                onNeedToRequestPermissions = { permissions ->
                    requestPermissions(permissions, REQUEST_CODE_PROMPT_PERMISSIONS)
                }),
            owner = this,
            view = view
        )

        sessionFeature.set(
            feature = SessionFeature(
                sessionManager,
                SessionUseCases(sessionManager),
                view.engineView,
                customTabSessionId
            ),
            owner = this,
            view = view
        )

        val accentHighContrastColor = ThemeManager.resolveAttribute(R.attr.accentHighContrast, requireContext())

        sitePermissionsFeature.set(
            feature = SitePermissionsFeature(
                context = requireContext(),
                sessionManager = sessionManager,
                fragmentManager = requireFragmentManager(),
                promptsStyling = SitePermissionsFeature.PromptsStyling(
                    gravity = getAppropriateLayoutGravity(),
                    shouldWidthMatchParent = true,
                    positiveButtonBackgroundColor = accentHighContrastColor,
                    positiveButtonTextColor = R.color.photonWhite
                ),
                sessionId = customTabSessionId
            ) { permissions ->
                requestPermissions(permissions, REQUEST_CODE_APP_PERMISSIONS)
            },
            owner = this,
            view = view
        )

        fullScreenFeature.set(
            feature = FullScreenFeature(
                sessionManager,
                SessionUseCases(sessionManager),
                customTabSessionId
            ) {
                if (it) {
                    FenixSnackbar.make(view.rootView, Snackbar.LENGTH_SHORT)
                        .setText(getString(R.string.full_screen_notification))
                        .show()
                    activity?.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_USER_LANDSCAPE
                    activity?.enterToImmersiveMode()
                    toolbar.visibility = View.GONE
                    nestedScrollQuickAction.visibility = View.GONE
                } else {
                    activity?.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_USER
                    activity?.exitImmersiveModeIfNeeded()
                    (activity as HomeActivity).let { activity: HomeActivity ->
                        ThemeManager.applyStatusBarTheme(
                            activity.window,
                            activity.themeManager,
                            activity
                        )
                    }
                    toolbar.visibility = View.VISIBLE
                    nestedScrollQuickAction.visibility = View.VISIBLE
                }
                changeEngineMargins(swipeRefresh = view.swipeRefresh, inFullScreen = it)
            },
            owner = this,
            view = view
        )

        thumbnailsFeature.set(
            feature = ThumbnailsFeature(
                requireContext(),
                view.engineView,
                requireComponents.core.sessionManager
            ),
            owner = this,
            view = view
        )

        if (FeatureFlags.pullToRefreshEnabled) {
            val primaryTextColor = ThemeManager.resolveAttribute(R.attr.primaryText, requireContext())
            view.swipeRefresh.setColorSchemeColors(primaryTextColor)
            swipeRefreshFeature.set(
                feature = SwipeRefreshFeature(
                    requireComponents.core.sessionManager,
                    requireComponents.useCases.sessionUseCases.reload,
                    view.swipeRefresh,
                    customTabSessionId
                ),
                owner = this,
                view = view
            )
        } else {
            // Disable pull to refresh
            view.swipeRefresh.setOnChildScrollUpCallback { _, _ -> true }
        }

        if ((activity as HomeActivity).browsingModeManager.isPrivate) {
            // We need to update styles for private mode programmatically for now:
            // https://github.com/mozilla-mobile/android-components/issues/3400
            themeReaderViewControlsForPrivateMode(view.readerViewControlsBar)
        }

        readerViewFeature.set(
            feature = ReaderViewFeature(
                requireContext(),
                requireComponents.core.engine,
                requireComponents.core.sessionManager,
                view.readerViewControlsBar
            ) { available ->
                if (available) { requireComponents.analytics.metrics.track(Event.ReaderModeAvailable) }

                browserStore.apply {
                    dispatch(QuickActionSheetAction.ReadableStateChange(available))
                    dispatch(QuickActionSheetAction.ReaderActiveStateChange(
                        sessionManager.selectedSession?.readerMode ?: false
                    ))
                }
            },
            owner = this,
            view = view
        )

        customTabSessionId?.let {
            customTabsIntegration.set(
                feature = CustomTabsIntegration(
                    requireContext(),
                    requireComponents.core.sessionManager,
                    toolbar,
                    it,
                    activity,
                    view.nestedScrollQuickAction,
                    view.swipeRefresh,
                    onItemTapped = { browserInteractor.onBrowserToolbarMenuItemTapped(it) }
                ),
                owner = this,
                view = view)
        }

        browserToolbarView.view.setOnSiteSecurityClickedListener {
            showQuickSettingsDialog()
        }

        consumeFrom(browserStore) {
            quickActionSheetView.update(it)
            browserToolbarView.update(it)
        }
    }

    private fun themeReaderViewControlsForPrivateMode(view: View) = with(view) {
        listOf(
            R.id.mozac_feature_readerview_font_size_decrease,
            R.id.mozac_feature_readerview_font_size_increase
        ).map {
            findViewById<Button>(it)
        }.forEach {
            it.setTextColor(ContextCompat.getColorStateList(context, R.color.readerview_private_button_color))
        }

        listOf(
            R.id.mozac_feature_readerview_font_serif,
            R.id.mozac_feature_readerview_font_sans_serif
        ).map {
            findViewById<RadioButton>(it)
        }.forEach {
            it.setTextColor(ContextCompat.getColorStateList(context, R.color.readerview_private_radio_color))
        }
    }

    private fun changeEngineMargins(swipeRefresh: View, inFullScreen: Boolean) {
        swipeRefresh.apply {
            val toolbarAndQASSize = resources.getDimension(R.dimen.toolbar_and_qab_height).toInt()
            val toolbarSize = resources.getDimension(R.dimen.browser_toolbar_height).toInt()
            val (topMargin, bottomMargin) = when {
                inFullScreen -> Pair(0, 0)
                customTabSessionId == null -> Pair(0, toolbarAndQASSize)
                else -> Pair(toolbarSize, 0)
            }
            (layoutParams as CoordinatorLayout.LayoutParams).apply {
                setMargins(
                    0,
                    topMargin,
                    0,
                    bottomMargin
                )
            }
        }
    }

    @SuppressWarnings("ComplexMethod")
    override fun onResume() {
        super.onResume()
        sessionObserver = subscribeToSession()
        sessionManagerObserver = subscribeToSessions()
        tabCollectionObserver = subscribeToTabCollections()
        requireComponents.core.tabCollectionStorage.register(collectionStorageObserver, this)

        getSessionById()?.let { updateBookmarkState(it) }

        if (getSessionById() == null) findNavController(this).popBackStack(R.id.homeFragment, false)
        context?.components?.core?.let {
            val preferredColorScheme = it.getPreferredColorScheme()
            if (it.engine.settings.preferredColorScheme != preferredColorScheme) {
                it.engine.settings.preferredColorScheme = preferredColorScheme
                context?.components?.useCases?.sessionUseCases?.reload?.invoke()
            }
        }
        getSessionById()?.let { (activity as HomeActivity).updateThemeForSession(it) }
        (activity as AppCompatActivity).supportActionBar?.hide()

        assignSitePermissionsRules()
    }

    private suspend fun bookmarkTapped(session: Session) = withContext(IO) {
        val bookmarksStorage = requireComponents.core.bookmarksStorage
        val existing = bookmarksStorage.getBookmarksWithUrl(session.url).firstOrNull { it.url == session.url }
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
                                BrowserFragmentDirections.actionBrowserFragmentToBookmarkEditFragment(guid)
                            )
                        }
                        .setText(getString(R.string.bookmark_saved_snackbar))
                        .show()
                }
            }
        }
    }

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        outState.putString(KEY_CUSTOM_TAB_SESSION_ID, customTabSessionId)
    }

    override fun onViewStateRestored(savedInstanceState: Bundle?) {
        super.onViewStateRestored(savedInstanceState)
        savedInstanceState?.getString(KEY_CUSTOM_TAB_SESSION_ID)?.let {
            if (requireComponents.core.sessionManager.findSessionById(it)?.customTabConfig != null) {
                customTabSessionId = it
            }
        }
    }

    override fun onStop() {
        super.onStop()
        tabCollectionObserver?.let {
            requireComponents.core.tabCollectionStorage.getCollections().removeObserver(it)
        }
        sessionObserver?.let {
            getSessionById()?.unregister(it)
        }
        sessionManagerObserver?.let {
            requireComponents.core.sessionManager.unregister(it)
        }
    }

    override fun onBackPressed(): Boolean {
        return when {
            findInPageIntegration.onBackPressed() -> true
            fullScreenFeature.onBackPressed() -> true
            readerViewFeature.onBackPressed() -> true
            sessionFeature.onBackPressed() -> true
            customTabsIntegration.onBackPressed() -> true
            else -> {
                removeSessionIfNeeded()
                false
            }
        }
    }

    override fun onPause() {
        super.onPause()
        fullScreenFeature.onBackPressed()
    }

    private fun removeSessionIfNeeded() {
        val session = getSessionById() ?: return
        if (session.source == Session.Source.ACTION_VIEW) requireComponents.core.sessionManager.remove(session)
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<String>,
        grantResults: IntArray
    ) {
        when (requestCode) {
            REQUEST_CODE_DOWNLOAD_PERMISSIONS -> downloadsFeature.withFeature {
                it.onPermissionsResult(permissions, grantResults)
            }
            REQUEST_CODE_PROMPT_PERMISSIONS -> promptsFeature.withFeature {
                it.onPermissionsResult(permissions, grantResults)
            }
            REQUEST_CODE_APP_PERMISSIONS -> sitePermissionsFeature.withFeature {
                it.onPermissionsResult(permissions, grantResults)
            }
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        promptsFeature.withFeature { it.onActivityResult(requestCode, resultCode, data) }
    }

    private fun assignSitePermissionsRules() {
        val settings = Settings.getInstance(requireContext())

        val rules: SitePermissionsRules = settings.getSitePermissionsCustomSettingsRules()

        sitePermissionsFeature.withFeature {
            it.sitePermissionsRules = rules
        }
    }

    private fun showQuickSettingsDialog() {
        val session = getSessionById() ?: return
        lifecycleScope.launch(IO) {
            val host = session.url.toUri().host
            val sitePermissions: SitePermissions? = host?.let {
                val storage = requireContext().components.core.permissionStorage
                storage.findSitePermissionsBy(it)
            }

            launch(Main) {
                view?.let {
                    val directions = BrowserFragmentDirections.actionBrowserFragmentToQuickSettingsSheetDialogFragment(
                        sessionId = session.id,
                        url = session.url,
                        isSecured = session.securityInfo.secure,
                        isTrackingProtectionOn = session.trackerBlockingEnabled,
                        sitePermissions = sitePermissions,
                        gravity = getAppropriateLayoutGravity()
                    )
                    nav(R.id.browserFragment, directions)
                }
            }
        }
    }

    private fun getSessionById(): Session? {
        val sessionManager = context?.components?.core?.sessionManager ?: return null
        return if (customTabSessionId != null) {
            sessionManager.findSessionById(customTabSessionId!!)
        } else {
            sessionManager.selectedSession
        }
    }

    private fun getAppropriateLayoutGravity() = if (customTabSessionId != null) Gravity.TOP else Gravity.BOTTOM

    private fun subscribeToTabCollections() =
        Observer<List<TabCollection>> {
            requireComponents.core.tabCollectionStorage.cachedTabCollections = it
            getManagedEmitter<SessionControlChange>().onNext(SessionControlChange.CollectionsChange(it))
        }.also { observer ->
            requireComponents.core.tabCollectionStorage.getCollections().observe(this, observer)
        }

    private fun subscribeToSession(): Session.Observer {
        return object : Session.Observer {
            override fun onLoadingStateChanged(session: Session, loading: Boolean) {
                if (!loading) {
                    updateBookmarkState(session)
                    browserStore.dispatch(QuickActionSheetAction.BounceNeededChange)
                }
            }

            override fun onUrlChanged(session: Session, url: String) {
                updateBookmarkState(session)
                updateAppLinksState(session)
            }
        }.also { observer -> getSessionById()?.register(observer, this) }
    }

    private fun subscribeToSessions(): SessionManager.Observer {
        return object : SessionManager.Observer {
            override fun onSessionSelected(session: Session) {
                (activity as HomeActivity).updateThemeForSession(session)
                updateBookmarkState(session)
            }
        }.also { requireComponents.core.sessionManager.register(it, this) }
    }

    private fun findBookmarkedURL(session: Session?): Boolean {
        session?.let {
            return runBlocking {
                try {
                    val url = URL(it.url).toString()
                    val list = requireComponents.core.bookmarksStorage.getBookmarksWithUrl(url)
                    list.isNotEmpty() && list[0].url == url
                } catch (e: MalformedURLException) {
                    false
                }
            }
        }
        return false
    }

    private fun updateBookmarkState(session: Session) {
        findBookmarkJob?.cancel()
        findBookmarkJob = lifecycleScope.launch(IO) {
            val found = findBookmarkedURL(session)
            withContext(Main) {
                browserStore.dispatch(QuickActionSheetAction.BookmarkedStateChange(found))
            }
        }
    }

    private fun updateAppLinksState(session: Session) {
        val url = session.url
        val appLinks = requireComponents.useCases.appLinksUseCases.appLinkRedirect
        browserStore.dispatch(QuickActionSheetAction.AppLinkStateChange(appLinks.invoke(url).hasExternalApp()))
    }

    private val collectionStorageObserver = object : TabCollectionStorage.Observer {
        override fun onCollectionCreated(title: String, sessions: List<Session>) {
            showTabSavedToCollectionSnackbar()
        }

        override fun onTabsAdded(tabCollection: TabCollection, sessions: List<Session>) {
            showTabSavedToCollectionSnackbar()
        }
    }

    private fun showTabSavedToCollectionSnackbar() {
        view?.let { view ->
            FenixSnackbar.make(view, Snackbar.LENGTH_SHORT)
                .setText(view.context.getString(R.string.create_collection_tab_saved))
                .setAnchorView(browserToolbarView.view)
                .show()
        }
    }

    private fun shareUrl(url: String) {
        val directions = BrowserFragmentDirections.actionBrowserFragmentToShareFragment(url = url)
        nav(R.id.browserFragment, directions)
    }

    companion object {
        private const val SHARED_TRANSITION_MS = 200L
        private const val TAB_ITEM_TRANSITION_NAME = "tab_item"
        private const val KEY_CUSTOM_TAB_SESSION_ID = "custom_tab_session_id"
        private const val REQUEST_CODE_DOWNLOAD_PERMISSIONS = 1
        private const val REQUEST_CODE_PROMPT_PERMISSIONS = 2
        private const val REQUEST_CODE_APP_PERMISSIONS = 3
        const val REPORT_SITE_ISSUE_URL =
            "https://webcompat.com/issues/new?url=%s&label=browser-fenix"
    }
}
