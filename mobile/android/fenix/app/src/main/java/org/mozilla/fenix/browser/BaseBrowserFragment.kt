/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.content.Context
import android.content.Intent
import android.content.pm.ActivityInfo
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.os.Bundle
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.CallSuper
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.net.toUri
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.NavDirections
import androidx.navigation.fragment.findNavController
import com.google.android.material.snackbar.Snackbar
import kotlinx.android.synthetic.main.fragment_browser.*
import kotlinx.android.synthetic.main.fragment_browser.view.*
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.Job
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.accounts.FxaCapability
import mozilla.components.feature.accounts.FxaWebChannelFeature
import mozilla.components.feature.app.links.AppLinksFeature
import mozilla.components.feature.contextmenu.ContextMenuCandidate
import mozilla.components.feature.contextmenu.ContextMenuFeature
import mozilla.components.feature.downloads.AbstractFetchDownloadService
import mozilla.components.feature.downloads.DownloadsFeature
import mozilla.components.feature.downloads.manager.FetchDownloadManager
import mozilla.components.feature.intent.ext.EXTRA_SESSION_ID
import mozilla.components.feature.prompts.PromptFeature
import mozilla.components.feature.readerview.ReaderViewFeature
import mozilla.components.feature.session.FullScreenFeature
import mozilla.components.feature.session.SessionFeature
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.session.SwipeRefreshFeature
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.feature.sitepermissions.SitePermissionsFeature
import mozilla.components.feature.sitepermissions.SitePermissionsRules
import mozilla.components.support.base.feature.PermissionsFeature
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.ktx.android.view.exitImmersiveModeIfNeeded
import org.mozilla.fenix.Experiments
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.IntentReceiverActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.readermode.DefaultReaderModeController
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.FindInPageIntegration
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.toolbar.BrowserFragmentState
import org.mozilla.fenix.components.toolbar.BrowserFragmentStore
import org.mozilla.fenix.components.toolbar.BrowserInteractor
import org.mozilla.fenix.components.toolbar.BrowserToolbarView
import org.mozilla.fenix.components.toolbar.BrowserToolbarViewInteractor
import org.mozilla.fenix.components.toolbar.DefaultBrowserToolbarController
import org.mozilla.fenix.components.toolbar.ToolbarIntegration
import org.mozilla.fenix.downloads.DownloadNotificationBottomSheetDialog
import org.mozilla.fenix.downloads.DownloadService
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.enterToImmersiveMode
import org.mozilla.fenix.ext.getRootView
import org.mozilla.fenix.ext.hideToolbar
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.sessionsOfType
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.isInExperiment
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.theme.ThemeManager

/**
 * Base fragment extended by [BrowserFragment].
 * This class only contains shared code focused on the main browsing content.
 * UI code specific to the app or to custom tabs can be found in the subclasses.
 */
@Suppress("TooManyFunctions", "LargeClass")
abstract class BaseBrowserFragment : Fragment(), UserInteractionHandler, SessionManager.Observer {
    protected lateinit var browserFragmentStore: BrowserFragmentStore
    protected lateinit var browserInteractor: BrowserToolbarViewInteractor
    protected lateinit var browserToolbarView: BrowserToolbarView

    protected val readerViewFeature = ViewBoundFeatureWrapper<ReaderViewFeature>()

    private val sessionFeature = ViewBoundFeatureWrapper<SessionFeature>()
    private val contextMenuFeature = ViewBoundFeatureWrapper<ContextMenuFeature>()
    private val downloadsFeature = ViewBoundFeatureWrapper<DownloadsFeature>()
    private val appLinksFeature = ViewBoundFeatureWrapper<AppLinksFeature>()
    private val promptsFeature = ViewBoundFeatureWrapper<PromptFeature>()
    private val findInPageIntegration = ViewBoundFeatureWrapper<FindInPageIntegration>()
    private val toolbarIntegration = ViewBoundFeatureWrapper<ToolbarIntegration>()
    private val sitePermissionsFeature = ViewBoundFeatureWrapper<SitePermissionsFeature>()
    private val fullScreenFeature = ViewBoundFeatureWrapper<FullScreenFeature>()
    private val swipeRefreshFeature = ViewBoundFeatureWrapper<SwipeRefreshFeature>()
    private val webchannelIntegration = ViewBoundFeatureWrapper<FxaWebChannelFeature>()

    var customTabSessionId: String? = null

    private var browserInitialized: Boolean = false
    private var initUIJob: Job? = null

    @CallSuper
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        require(arguments != null)
        customTabSessionId = arguments?.getString(EXTRA_SESSION_ID)

        val view = inflater.inflate(R.layout.fragment_browser, container, false)

        val activity = activity as HomeActivity
        activity.themeManager.applyStatusBarTheme(activity)

        browserFragmentStore = StoreProvider.get(this) {
            BrowserFragmentStore(
                BrowserFragmentState()
            )
        }

        return view
    }

    final override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        browserInitialized = initializeUI(view) != null
    }

    @Suppress("ComplexMethod", "LongMethod")
    @CallSuper
    protected open fun initializeUI(view: View): Session? {
        val context = requireContext()
        val sessionManager = context.components.core.sessionManager
        val store = context.components.core.store

        return getSessionById()?.also { session ->

            // We need to show the snackbar while the browsing data is deleting(if "Delete
            // browsing data on quit" is activated). After the deletion is over, the snackbar
            // is dismissed.
            val snackbar: FenixSnackbar? = requireActivity().getRootView()?.let { v ->
                FenixSnackbar.make(v, Snackbar.LENGTH_INDEFINITE)
                    .setText(v.context.getString(R.string.deleting_browsing_data_in_progress))
            }

            val browserToolbarController = DefaultBrowserToolbarController(
                store = browserFragmentStore,
                activity = requireActivity(),
                snackbar = snackbar,
                navController = findNavController(),
                readerModeController = DefaultReaderModeController(readerViewFeature),
                browsingModeManager = (activity as HomeActivity).browsingModeManager,
                sessionManager = requireComponents.core.sessionManager,
                findInPageLauncher = { findInPageIntegration.withFeature { it.launch() } },
                browserLayout = view.browserLayout,
                engineView = engineView,
                swipeRefresh = swipeRefresh,
                adjustBackgroundAndNavigate = ::adjustBackgroundAndNavigate,
                customTabSession = customTabSessionId?.let { sessionManager.findSessionById(it) },
                getSupportUrl = {
                    SupportUtils.getSumoURLForTopic(
                        context,
                        SupportUtils.SumoTopic.HELP
                    )
                },
                openInFenixIntent = Intent(context, IntentReceiverActivity::class.java).apply {
                    action = Intent.ACTION_VIEW
                    flags = Intent.FLAG_ACTIVITY_NEW_TASK
                },
                bookmarkTapped = { lifecycleScope.launch { bookmarkTapped(it) } },
                scope = lifecycleScope,
                tabCollectionStorage = requireComponents.core.tabCollectionStorage
            )

            browserInteractor = BrowserInteractor(
                browserToolbarController = browserToolbarController
            )

            browserToolbarView = BrowserToolbarView(
                container = view.browserLayout,
                shouldUseBottomToolbar = context.settings().shouldUseBottomToolbar,
                interactor = browserInteractor,
                customTabSession = customTabSessionId?.let { sessionManager.findSessionById(it) }
            )

            toolbarIntegration.set(
                feature = browserToolbarView.toolbarIntegration,
                owner = this,
                view = view
            )

            findInPageIntegration.set(
                feature = FindInPageIntegration(
                    store = store,
                    sessionId = customTabSessionId,
                    stub = view.stubFindInPage,
                    engineView = view.engineView,
                    toolbar = browserToolbarView.view
                ),
                owner = this,
                view = view
            )

            browserToolbarView.view.display.setOnSiteSecurityClickedListener {
                showQuickSettingsDialog()
            }

            browserToolbarView.view.display.setOnTrackingProtectionClickedListener {
                context.metrics.track(Event.TrackingProtectionIconPressed)
                showTrackingProtectionPanel()
            }

            contextMenuFeature.set(
                feature = ContextMenuFeature(
                    fragmentManager = parentFragmentManager,
                    store = store,
                    candidates = getContextMenuCandidates(context, view),
                    engineView = view.engineView,
                    useCases = context.components.useCases.contextMenuUseCases,
                    customTabId = customTabSessionId
                ),
                owner = this,
                view = view
            )

            val downloadFeature = DownloadsFeature(
                context.applicationContext,
                store = store,
                useCases = context.components.useCases.downloadUseCases,
                fragmentManager = childFragmentManager,
                customTabId = customTabSessionId,
                downloadManager = FetchDownloadManager(
                    context.applicationContext,
                    DownloadService::class
                ),
                promptsStyling = DownloadsFeature.PromptsStyling(
                    gravity = Gravity.BOTTOM,
                    shouldWidthMatchParent = true,
                    positiveButtonBackgroundColor = ThemeManager.resolveAttribute(
                        R.attr.accent,
                        context
                    ),
                    positiveButtonTextColor = ThemeManager.resolveAttribute(
                        R.attr.contrastText,
                        context
                    ),
                    positiveButtonRadius = (resources.getDimensionPixelSize(R.dimen.tab_corner_radius)).toFloat()
                ),
                onNeedToRequestPermissions = { permissions ->
                    requestPermissions(permissions, REQUEST_CODE_DOWNLOAD_PERMISSIONS)
                }
            )

            downloadFeature.onDownloadStopped = { download, _, downloadJobStatus ->
                // If the download is just paused, don't show any in-app notification
                if (downloadJobStatus == AbstractFetchDownloadService.DownloadJobStatus.COMPLETED ||
                    downloadJobStatus == AbstractFetchDownloadService.DownloadJobStatus.FAILED
                ) {
                    val dialog = DownloadNotificationBottomSheetDialog(
                        context = context,
                        didFail = downloadJobStatus == AbstractFetchDownloadService.DownloadJobStatus.FAILED,
                        download = download,
                        tryAgain = downloadFeature::tryAgain,
                        onCannotOpenFile = {
                            FenixSnackbar.make(view, Snackbar.LENGTH_SHORT)
                                .setText(context.getString(R.string.mozac_feature_downloads_could_not_open_file))
                                .setAnchorView(browserToolbarView.getSnackbarAnchor())
                                .show()
                        }

                    )
                    dialog.show()
                }
            }

            downloadsFeature.set(
                downloadFeature,
                owner = this,
                view = view
            )

            appLinksFeature.set(
                feature = AppLinksFeature(
                    context,
                    sessionManager = sessionManager,
                    sessionId = customTabSessionId,
                    interceptLinkClicks = true,
                    fragmentManager = parentFragmentManager
                ),
                owner = this,
                view = view
            )

            promptsFeature.set(
                feature = PromptFeature(
                    fragment = this,
                    store = store,
                    customTabId = customTabSessionId,
                    fragmentManager = parentFragmentManager,
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

            val accentHighContrastColor =
                ThemeManager.resolveAttribute(R.attr.accentHighContrast, context)

            sitePermissionsFeature.set(
                feature = SitePermissionsFeature(
                    context = context,
                    sessionManager = sessionManager,
                    fragmentManager = parentFragmentManager,
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
                ) { inFullScreen ->
                    if (inFullScreen) {
                        FenixSnackbar.make(view.rootView, Snackbar.LENGTH_SHORT)
                            .setText(getString(R.string.full_screen_notification))
                            .show()
                        activity?.requestedOrientation =
                            ActivityInfo.SCREEN_ORIENTATION_USER_LANDSCAPE
                        activity?.enterToImmersiveMode()
                        browserToolbarView.view.visibility = View.GONE
                    } else {
                        activity?.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_USER
                        activity?.exitImmersiveModeIfNeeded()
                        (activity as? HomeActivity)?.let { activity ->
                            activity.themeManager.applyStatusBarTheme(activity)
                        }
                        browserToolbarView.view.visibility = View.VISIBLE
                    }
                    updateLayoutMargins(inFullScreen)
                },
                owner = this,
                view = view
            )

            session.register(observer = object : Session.Observer {
                override fun onLoadRequest(
                    session: Session,
                    url: String,
                    triggeredByRedirect: Boolean,
                    triggeredByWebContent: Boolean
                ): Boolean {
                    browserToolbarView.expand()
                    return false
                }
            }, owner = viewLifecycleOwner)

            @Suppress("ConstantConditionIf")
            if (FeatureFlags.pullToRefreshEnabled) {
                val primaryTextColor =
                    ThemeManager.resolveAttribute(R.attr.primaryText, context)
                view.swipeRefresh.setColorSchemeColors(primaryTextColor)
                swipeRefreshFeature.set(
                    feature = SwipeRefreshFeature(
                        sessionManager,
                        context.components.useCases.sessionUseCases.reload,
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

            if (!requireContext().isInExperiment(Experiments.asFeatureWebChannelsDisabled)) {
                webchannelIntegration.set(
                    feature = FxaWebChannelFeature(
                        requireContext(),
                        customTabSessionId,
                        requireComponents.core.engine,
                        requireComponents.core.sessionManager,
                        requireComponents.backgroundServices.accountManager,
                        setOf(FxaCapability.CHOOSE_WHAT_TO_SYNC)
                    ),
                    owner = this,
                    view = view
                )
            }

            (activity as HomeActivity).updateThemeForSession(session)
        }
    }

    /**
     * Returns a list of context menu items [ContextMenuCandidate] for the context menu
     */
    protected abstract fun getContextMenuCandidates(
        context: Context,
        view: View
    ): List<ContextMenuCandidate>

    private fun adjustBackgroundAndNavigate(directions: NavDirections) {
        context?.let {
            swipeRefresh?.background = ColorDrawable(Color.TRANSPARENT)
            engineView?.asView()?.visibility = View.GONE
            findNavController().nav(R.id.browserFragment, directions)
        }
    }

    @CallSuper
    override fun onSessionSelected(session: Session) {
        (activity as HomeActivity).updateThemeForSession(session)
        if (!browserInitialized) {
            // Initializing a new coroutineScope to avoid ConcurrentModificationException in ObserverRegistry
            // This will be removed when ObserverRegistry is deprecated by browser-state.
            initUIJob = MainScope().launch {
                view?.let {
                    browserInitialized = initializeUI(it) != null
                }
            }
        }
    }

    @CallSuper
    override fun onStart() {
        super.onStart()
        requireComponents.core.sessionManager.register(this, this, autoPause = true)
    }

    @CallSuper
    override fun onResume() {
        super.onResume()
        val components = requireComponents

        val preferredColorScheme = components.core.getPreferredColorScheme()
        if (components.core.engine.settings.preferredColorScheme != preferredColorScheme) {
            components.core.engine.settings.preferredColorScheme = preferredColorScheme
            components.useCases.sessionUseCases.reload()
        }
        hideToolbar()

        assignSitePermissionsRules()
    }

    @CallSuper
    final override fun onPause() {
        super.onPause()
        fullScreenFeature.onBackPressed()
    }

    @CallSuper
    override fun onStop() {
        super.onStop()
        initUIJob?.cancel()
    }

    @CallSuper
    override fun onBackPressed(): Boolean {
        return findInPageIntegration.onBackPressed() ||
                fullScreenFeature.onBackPressed() ||
                sessionFeature.onBackPressed() ||
                removeSessionIfNeeded()
    }

    /**
     * Saves the external app session ID to be restored later in [onViewStateRestored].
     */
    final override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        outState.putString(KEY_CUSTOM_TAB_SESSION_ID, customTabSessionId)
    }

    /**
     * Retrieves the external app session ID saved by [onSaveInstanceState].
     */
    final override fun onViewStateRestored(savedInstanceState: Bundle?) {
        super.onViewStateRestored(savedInstanceState)
        savedInstanceState?.getString(KEY_CUSTOM_TAB_SESSION_ID)?.let {
            if (requireComponents.core.sessionManager.findSessionById(it)?.customTabConfig != null) {
                customTabSessionId = it
            }
        }
    }

    /**
     * Forwards permission grant results to one of the features.
     */
    final override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<String>,
        grantResults: IntArray
    ) {
        val feature: PermissionsFeature? = when (requestCode) {
            REQUEST_CODE_DOWNLOAD_PERMISSIONS -> downloadsFeature.get()
            REQUEST_CODE_PROMPT_PERMISSIONS -> promptsFeature.get()
            REQUEST_CODE_APP_PERMISSIONS -> sitePermissionsFeature.get()
            else -> null
        }
        feature?.onPermissionsResult(permissions, grantResults)
    }

    /**
     * Forwards activity results to the prompt feature.
     */
    final override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        promptsFeature.withFeature { it.onActivityResult(requestCode, resultCode, data) }
    }

    /**
     * Removes the session if it was opened by an ACTION_VIEW intent
     * or if it has no more history
     */
    protected open fun removeSessionIfNeeded(): Boolean {
        getSessionById()?.let { session ->
            val sessionManager = requireComponents.core.sessionManager
            if (session.source == Session.Source.ACTION_VIEW) {
                sessionManager.remove(session)
            } else {
                val isLastSession =
                    sessionManager.sessionsOfType(private = session.private).count() == 1
                sessionManager.remove(session, session.hasParentSession)
                val goToOverview = isLastSession || !session.hasParentSession
                return !goToOverview
            }
        }
        return false
    }

    protected abstract fun navToQuickSettingsSheet(
        session: Session,
        sitePermissions: SitePermissions?
    )

    protected abstract fun navToTrackingProtectionPanel(session: Session)

    /**
     * Returns the top and bottom margins.
     */
    protected abstract fun getEngineMargins(): Pair<Int, Int>

    /**
     * Returns the layout [android.view.Gravity] for the quick settings dialog.
     */
    protected abstract fun getAppropriateLayoutGravity(): Int

    protected fun updateLayoutMargins(inFullScreen: Boolean) {
        view?.swipeRefresh?.apply {
            val (topMargin, bottomMargin) = if (inFullScreen) 0 to 0 else getEngineMargins()
            (layoutParams as CoordinatorLayout.LayoutParams).setMargins(
                0,
                topMargin,
                0,
                bottomMargin
            )
        }
    }

    /**
     * Updates the site permissions rules based on user settings.
     */
    private fun assignSitePermissionsRules() {
        val settings = requireContext().settings()

        val rules: SitePermissionsRules = settings.getSitePermissionsCustomSettingsRules()

        sitePermissionsFeature.withFeature {
            it.sitePermissionsRules = rules
        }
    }

    /**
     * Displays the quick settings dialog,
     * which lets the user control tracking protection and site settings.
     */
    private fun showQuickSettingsDialog() {
        val session = getSessionById() ?: return
        lifecycleScope.launch(Main) {
            val sitePermissions: SitePermissions? = withContext(IO) {
                session.url.toUri().host?.let { host ->
                    val storage = requireContext().components.core.permissionStorage
                    storage.findSitePermissionsBy(host)
                }
            }

            view?.let {
                navToQuickSettingsSheet(session, sitePermissions)
            }
        }
    }

    private fun showTrackingProtectionPanel() {
        val session = getSessionById() ?: return
        view?.let {
            navToTrackingProtectionPanel(session)
        }
    }

    /**
     * Returns the current session.
     */
    protected fun getSessionById(): Session? {
        val sessionManager = context?.components?.core?.sessionManager ?: return null
        val localCustomTabId = customTabSessionId
        return if (localCustomTabId != null) {
            sessionManager.findSessionById(localCustomTabId)
        } else {
            sessionManager.selectedSession
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
                requireComponents.analytics.metrics.track(Event.AddBookmark)

                view?.let { view ->
                    FenixSnackbar.make(view, Snackbar.LENGTH_LONG)
                        .setAnchorView(browserToolbarView.getSnackbarAnchor())
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

    companion object {
        private const val KEY_CUSTOM_TAB_SESSION_ID = "custom_tab_session_id"
        private const val REQUEST_CODE_DOWNLOAD_PERMISSIONS = 1
        private const val REQUEST_CODE_PROMPT_PERMISSIONS = 2
        private const val REQUEST_CODE_APP_PERMISSIONS = 3
    }
}
