/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModelProviders
import androidx.navigation.Navigation
import androidx.navigation.fragment.NavHostFragment.findNavController
import androidx.transition.TransitionInflater
import com.google.android.material.snackbar.Snackbar
import kotlinx.android.synthetic.main.component_search.*
import kotlinx.android.synthetic.main.fragment_browser.*
import kotlinx.android.synthetic.main.fragment_browser.view.*
import kotlinx.android.synthetic.main.fragment_search.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.app.links.AppLinksFeature
import mozilla.components.feature.contextmenu.ContextMenuCandidate
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
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.ktx.android.view.enterToImmersiveMode
import mozilla.components.support.ktx.android.view.exitImmersiveModeIfNeeded
import mozilla.components.support.ktx.kotlin.toUri
import org.mozilla.fenix.BrowsingModeManager
import org.mozilla.fenix.FenixViewModelProvider
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.IntentReceiverActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ThemeManager
import org.mozilla.fenix.collections.CreateCollectionViewModel
import org.mozilla.fenix.collections.SaveCollectionStep
import org.mozilla.fenix.collections.getStepForCollectionsSize
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.FindInPageIntegration
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.Event.BrowserMenuItemTapped.Item
import org.mozilla.fenix.components.toolbar.SearchAction
import org.mozilla.fenix.components.toolbar.SearchState
import org.mozilla.fenix.components.toolbar.ToolbarComponent
import org.mozilla.fenix.components.toolbar.ToolbarIntegration
import org.mozilla.fenix.components.toolbar.ToolbarMenu
import org.mozilla.fenix.components.toolbar.ToolbarUIView
import org.mozilla.fenix.components.toolbar.ToolbarViewModel
import org.mozilla.fenix.customtabs.CustomTabsIntegration
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.urlToTrimmedHost
import org.mozilla.fenix.home.sessioncontrol.Tab
import org.mozilla.fenix.lib.Do
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.getAutoDisposeObservable
import org.mozilla.fenix.mvi.getManagedEmitter
import org.mozilla.fenix.quickactionsheet.QuickActionAction
import org.mozilla.fenix.quickactionsheet.QuickActionChange
import org.mozilla.fenix.quickactionsheet.QuickActionComponent
import org.mozilla.fenix.quickactionsheet.QuickActionState
import org.mozilla.fenix.quickactionsheet.QuickActionViewModel
import org.mozilla.fenix.utils.ItsNotBrokenSnack
import org.mozilla.fenix.utils.Settings
import kotlin.coroutines.CoroutineContext

@SuppressWarnings("TooManyFunctions", "LargeClass")
class BrowserFragment : Fragment(), BackHandler, CoroutineScope {

    private lateinit var toolbarComponent: ToolbarComponent

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
    private lateinit var job: Job

    var customTabSessionId: String? = null

    override val coroutineContext: CoroutineContext get() = Dispatchers.IO + job

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        postponeEnterTransition()
        sharedElementEnterTransition =
            TransitionInflater.from(context).inflateTransition(android.R.transition.move).setDuration(
                SHARED_TRANSITION_MS
            )
        job = Job()
    }

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

        toolbarComponent = ToolbarComponent(
            view.browserLayout,
            ActionBusFactory.get(this), customTabSessionId,
            (activity as HomeActivity).browsingModeManager.isPrivate,
            false,
            search_engine_icon,
            FenixViewModelProvider.create(
                this,
                ToolbarViewModel::class.java
            ) {
                ToolbarViewModel(
                    SearchState("", getSessionById()?.searchTerms ?: "", isEditing = false)
                )
            }
        )

        startPostponedEnterTransition()

        QuickActionComponent(
            view.nestedScrollQuickAction,
            ActionBusFactory.get(this),
            FenixViewModelProvider.create(
                this,
                QuickActionViewModel::class.java
            ) {
                QuickActionViewModel(
                    QuickActionState(
                        readable = getSessionById()?.readerable ?: false,
                        bookmarked = findBookmarkedURL(getSessionById()),
                        readerActive = getSessionById()?.readerMode ?: false,
                        bounceNeeded = false
                    )
                )
            }
        )

        val activity = activity as HomeActivity
        ThemeManager.applyStatusBarTheme(activity.window, activity.themeManager, activity)

        return view
    }

    @Suppress("LongMethod")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        toolbarIntegration.set(
            feature = (toolbarComponent.uiView as ToolbarUIView).toolbarIntegration,
            owner = this,
            view = view
        )

        val sessionManager = requireComponents.core.sessionManager

        contextMenuFeature.set(
            feature = ContextMenuFeature(
                requireFragmentManager(),
                sessionManager,
                ContextMenuCandidate.defaultCandidates(
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

        findInPageIntegration.set(
            feature = FindInPageIntegration(
                requireComponents.core.sessionManager, view.findInPageView, view.engineView
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
                    FenixSnackbar.make(view.rootView, Snackbar.LENGTH_LONG)
                        .setText(getString(R.string.full_screen_notification))
                        .show()
                    activity?.enterToImmersiveMode()
                    toolbar.visibility = View.GONE
                    nestedScrollQuickAction.visibility = View.GONE
                } else {
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

        readerViewFeature.set(
            feature = ReaderViewFeature(
                requireContext(),
                requireComponents.core.engine,
                requireComponents.core.sessionManager,
                view.readerViewControlsBar
            ) {
                getManagedEmitter<QuickActionChange>().apply {
                    onNext(QuickActionChange.ReadableStateChange(it))
                    onNext(
                        QuickActionChange.ReaderActiveStateChange(
                            sessionManager.selectedSession?.readerMode ?: false
                        )
                    )
                }
            },
            owner = this,
            view = view
        )

        val actionEmitter = ActionBusFactory.get(this).getManagedEmitter(SearchAction::class.java)

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
                    onItemTapped = { actionEmitter.onNext(SearchAction.ToolbarMenuItemTapped(it)) }
                ),
                owner = this,
                view = view)
        }

        toolbarComponent.getView().setOnSiteSecurityClickedListener {
            showQuickSettingsDialog()
        }
    }

    override fun onResume() {
        if (getSessionById() == null) findNavController(this).popBackStack(R.id.homeFragment, false)
        super.onResume()
        context?.components?.core?.let {
            val preferredColorScheme = it.getPreferredColorScheme()
            if (it.engine.settings.preferredColorScheme != preferredColorScheme) {
                it.engine.settings.preferredColorScheme = preferredColorScheme
                context?.components?.useCases?.sessionUseCases?.reload?.invoke()
            }
        }
        getSessionById()?.let { (activity as HomeActivity).updateThemeForSession(it) }
        (activity as AppCompatActivity).supportActionBar?.hide()
    }

    @Suppress("ComplexMethod")
    override fun onStart() {
        super.onStart()
        sessionObserver = subscribeToSession()
        sessionManagerObserver = subscribeToSessions()
        getSessionById()?.let { updateBookmarkState(it) }
        getAutoDisposeObservable<SearchAction>()
            .subscribe {
                when (it) {
                    is SearchAction.ToolbarClicked -> {
                        Navigation
                            .findNavController(toolbarComponent.getView())
                            .navigate(
                                BrowserFragmentDirections.actionBrowserFragmentToSearchFragment(
                                    getSessionById()?.id
                                )
                            )

                        requireComponents.analytics.metrics.track(
                            Event.SearchBarTapped(Event.SearchBarTapped.Source.BROWSER)
                        )
                    }
                    is SearchAction.ToolbarMenuItemTapped -> {
                        trackToolbarItemInteraction(it)
                        handleToolbarItemInteraction(it)
                    }
                    is SearchAction.ToolbarLongClicked -> {
                        getSessionById()?.let { session ->
                            session.copyUrl(requireContext())
                            view?.let {
                                val snackbar = FenixSnackbar.make(it, Snackbar.LENGTH_LONG)
                                    .setText(resources.getString(R.string.url_copied))

                                if (!session.isCustomTabSession()) {
                                    snackbar.anchorView = nestedScrollQuickAction
                                }

                                snackbar.show()
                            }
                        }
                    }
                }
            }

        getAutoDisposeObservable<QuickActionAction>()
            .subscribe {
                when (it) {
                    is QuickActionAction.Opened -> {
                        requireComponents.analytics.metrics.track(Event.QuickActionSheetOpened)
                    }
                    is QuickActionAction.Closed -> {
                        requireComponents.analytics.metrics.track(Event.QuickActionSheetClosed)
                    }
                    is QuickActionAction.SharePressed -> {
                        requireComponents.analytics.metrics.track(Event.QuickActionSheetShareTapped)
                        getSessionById()?.let { session ->
                            shareUrl(session.url)
                        }
                    }
                    is QuickActionAction.DownloadsPressed -> {
                        requireComponents.analytics.metrics.track(Event.QuickActionSheetDownloadTapped)
                        ItsNotBrokenSnack(context!!).showSnackbar(issueNumber = "348")
                    }
                    is QuickActionAction.BookmarkPressed -> {
                        requireComponents.analytics.metrics.track(Event.QuickActionSheetBookmarkTapped)
                        bookmarkTapped()
                    }
                    is QuickActionAction.ReadPressed -> {
                        readerViewFeature.withFeature { feature ->
                            requireComponents.analytics.metrics.track(Event.QuickActionSheetReadTapped)
                            val actionEmitter = getManagedEmitter<QuickActionChange>()
                            val enabled = requireComponents.core.sessionManager.selectedSession?.readerMode ?: false
                            if (enabled) {
                                feature.hideReaderView()
                                actionEmitter.onNext(QuickActionChange.ReaderActiveStateChange(false))
                            } else {
                                feature.showReaderView()
                                actionEmitter.onNext(QuickActionChange.ReaderActiveStateChange(true))
                            }
                        }
                    }
                    is QuickActionAction.ReadAppearancePressed -> {
                        // TODO telemetry: https://github.com/mozilla-mobile/fenix/issues/2267
                        readerViewFeature.withFeature { feature ->
                            feature.showControls()
                        }
                    }
                }
            }
        assignSitePermissionsRules()
    }

    private fun bookmarkTapped() {
        getSessionById()?.let { session ->
            CoroutineScope(IO).launch {
                val components = requireComponents
                val existing = components.core.bookmarksStorage.getBookmarksWithUrl(session.url)
                val found = existing.isNotEmpty() && existing[0].url == session.url
                if (found) {
                    launch(Main) {
                        Navigation.findNavController(requireActivity(), R.id.container)
                            .navigate(
                                BrowserFragmentDirections
                                    .actionBrowserFragmentToBookmarkEditFragment(existing[0].guid)
                            )
                    }
                } else {
                    val guid = components.core.bookmarksStorage
                        .addItem(
                            BookmarkRoot.Mobile.id,
                            session.url,
                            session.title,
                            null
                        )
                    launch(Main) {
                        getManagedEmitter<QuickActionChange>()
                            .onNext(QuickActionChange.BookmarkedStateChange(true))
                        requireComponents.analytics.metrics.track(Event.AddBookmark)
                        view?.let {
                            FenixSnackbar.make(
                                it.rootView,
                                Snackbar.LENGTH_LONG
                            )
                                .setAnchorView(nestedScrollQuickAction)
                                .setAction(getString(R.string.edit_bookmark_snackbar_action)) {
                                    Navigation.findNavController(
                                        requireActivity(),
                                        R.id.container
                                    )
                                        .navigate(
                                            BrowserFragmentDirections
                                                .actionBrowserFragmentToBookmarkEditFragment(
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
        }
    }

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        outState.putString(KEY_CUSTOM_TAB_SESSION_ID, customTabSessionId)
    }

    override fun onViewStateRestored(savedInstanceState: Bundle?) {
        super.onViewStateRestored(savedInstanceState)
        customTabSessionId = savedInstanceState?.getString(KEY_CUSTOM_TAB_SESSION_ID)
    }

    override fun onStop() {
        super.onStop()
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
                it.onPermissionsResult(grantResults)
            }
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        promptsFeature.withFeature { it.onActivityResult(requestCode, resultCode, data) }
    }

    override fun onDestroy() {
        super.onDestroy()
        job.cancel()
    }

    // This method triggers the complexity warning. However it's actually not that hard to understand.
    @SuppressWarnings("ComplexMethod")
    private fun trackToolbarItemInteraction(action: SearchAction.ToolbarMenuItemTapped) {
        val item = when (action.item) {
            ToolbarMenu.Item.Back -> Item.BACK
            ToolbarMenu.Item.Forward -> Item.FORWARD
            ToolbarMenu.Item.Reload -> Item.RELOAD
            ToolbarMenu.Item.Stop -> Item.STOP
            ToolbarMenu.Item.Settings -> Item.SETTINGS
            ToolbarMenu.Item.Library -> Item.LIBRARY
            is ToolbarMenu.Item.RequestDesktop ->
                if (action.item.isChecked) Item.DESKTOP_VIEW_ON else Item.DESKTOP_VIEW_OFF
            ToolbarMenu.Item.NewPrivateTab -> Item.NEW_PRIVATE_TAB
            ToolbarMenu.Item.FindInPage -> Item.FIND_IN_PAGE
            ToolbarMenu.Item.ReportIssue -> Item.REPORT_SITE_ISSUE
            ToolbarMenu.Item.Help -> Item.HELP
            ToolbarMenu.Item.NewTab -> Item.NEW_TAB
            ToolbarMenu.Item.OpenInFenix -> Item.OPEN_IN_FENIX
            ToolbarMenu.Item.Share -> Item.SHARE
            ToolbarMenu.Item.SaveToCollection -> Item.SAVE_TO_COLLECTION
        }

        requireComponents.analytics.metrics.track(Event.BrowserMenuItemTapped(item))
    }

    // This method triggers the complexity warning. However it's actually not that hard to understand.
    @SuppressWarnings("ComplexMethod")
    private fun handleToolbarItemInteraction(action: SearchAction.ToolbarMenuItemTapped) {
        val sessionUseCases = requireComponents.useCases.sessionUseCases
        Do exhaustive when (action.item) {
            ToolbarMenu.Item.Back -> sessionUseCases.goBack.invoke(getSessionById())
            ToolbarMenu.Item.Forward -> sessionUseCases.goForward.invoke(getSessionById())
            ToolbarMenu.Item.Reload -> sessionUseCases.reload.invoke(getSessionById())
            ToolbarMenu.Item.Stop -> sessionUseCases.stopLoading.invoke(getSessionById())
            ToolbarMenu.Item.Settings -> Navigation.findNavController(toolbarComponent.getView())
                .navigate(BrowserFragmentDirections.actionBrowserFragmentToSettingsFragment())
            ToolbarMenu.Item.Library -> Navigation.findNavController(toolbarComponent.getView())
                .navigate(BrowserFragmentDirections.actionBrowserFragmentToLibraryFragment())
            is ToolbarMenu.Item.RequestDesktop -> sessionUseCases.requestDesktopSite.invoke(action.item.isChecked)
            ToolbarMenu.Item.Share -> getSessionById()?.let { session ->
                session.url.apply {
                    shareUrl(this)
                }
            }
            ToolbarMenu.Item.NewPrivateTab -> {
                val directions = BrowserFragmentDirections
                    .actionBrowserFragmentToSearchFragment(null)
                Navigation.findNavController(view!!).navigate(directions)
                (activity as HomeActivity).browsingModeManager.mode = BrowsingModeManager.Mode.Private
            }
            ToolbarMenu.Item.FindInPage -> {
                FindInPageIntegration.launch?.invoke()
                requireComponents.analytics.metrics.track(Event.FindInPageOpened)
            }
            ToolbarMenu.Item.ReportIssue -> getSessionById()?.let { session ->
                session.url.apply {
                    val reportUrl = String.format(REPORT_SITE_ISSUE_URL, this)
                    requireComponents.useCases.tabsUseCases.addTab.invoke(reportUrl)
                }
            }
            ToolbarMenu.Item.Help -> {
                // TODO Help #1016
                ItsNotBrokenSnack(context!!).showSnackbar(issueNumber = "1016")
            }
            ToolbarMenu.Item.NewTab -> {
                val directions = BrowserFragmentDirections
                    .actionBrowserFragmentToSearchFragment(null)
                Navigation.findNavController(view!!).navigate(directions)
                (activity as HomeActivity).browsingModeManager.mode =
                    BrowsingModeManager.Mode.Normal
            }
            ToolbarMenu.Item.SaveToCollection -> showSaveToCollection()
            ToolbarMenu.Item.OpenInFenix -> {
                // To not get a "Display Already Acquired" error we need to force remove the engineView here
                swipeRefresh?.removeView(engineView as View)
                val intent = Intent(context, IntentReceiverActivity::class.java)
                intent.action = Intent.ACTION_VIEW
                getSessionById()?.customTabConfig = null
                getSessionById()?.let {
                    requireComponents.core.sessionManager.select(it)
                }
                activity?.finish()
                startActivity(intent)
            }
        }
    }

    private fun showSaveToCollection() {
        getSessionById()?.let {
            val tabs = Tab(it.id, it.url, it.url.urlToTrimmedHost(), it.title)
            val viewModel = activity?.run {
                ViewModelProviders.of(this).get(CreateCollectionViewModel::class.java)
            }
            viewModel?.tabs = listOf(tabs)
            val selectedSet = mutableSetOf(tabs)
            viewModel?.selectedTabs = selectedSet
            viewModel?.tabCollections = requireComponents.core.tabCollectionStorage.cachedTabCollections.reversed()
            viewModel?.saveCollectionStep =
                viewModel?.tabCollections?.getStepForCollectionsSize() ?: SaveCollectionStep.SelectCollection
            viewModel?.snackbarAnchorView = nestedScrollQuickAction
            view?.let {
                val directions = BrowserFragmentDirections.actionBrowserFragmentToCreateCollectionFragment()
                Navigation.findNavController(it).navigate(directions)
            }
        }
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
        launch {
            val host = session.url.toUri()?.host
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
                    Navigation.findNavController(it).navigate(directions)
                }
            }
        }
    }

    private fun getSessionById(): Session? {
        val components = context?.components ?: return null
        return if (customTabSessionId != null) {
            components.core.sessionManager.findSessionById(customTabSessionId!!)
        } else {
            components.core.sessionManager.selectedSession
        }
    }

    private fun getAppropriateLayoutGravity() = if (customTabSessionId != null) {
        Gravity.TOP
    } else {
        Gravity.BOTTOM
    }

    private fun Session.copyUrl(context: Context) {
        val clipBoard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        clipBoard.primaryClip = ClipData.newPlainText(url, url)
    }

    private fun subscribeToSession(): Session.Observer {
        val observer = object : Session.Observer {
            override fun onLoadingStateChanged(session: Session, loading: Boolean) {
                if (!loading) {
                    updateBookmarkState(session)
                    getManagedEmitter<QuickActionChange>().onNext(QuickActionChange.BounceNeededChange)
                }

                super.onLoadingStateChanged(session, loading)
            }

            override fun onUrlChanged(session: Session, url: String) {
                super.onUrlChanged(session, url)
                updateBookmarkState(session)
            }
        }
        getSessionById()?.register(observer, this)
        return observer
    }

    private fun subscribeToSessions(): SessionManager.Observer {
        return object : SessionManager.Observer {
            override fun onSessionSelected(session: Session) {
                super.onSessionSelected(session)
                (activity as HomeActivity).updateThemeForSession(session)
            }
        }.also { requireComponents.core.sessionManager.register(it, this) }
    }

    private fun findBookmarkedURL(session: Session?): Boolean {
        session?.let {
            return runBlocking {
                val list = requireComponents.core.bookmarksStorage.getBookmarksWithUrl(it.url)
                list.isNotEmpty() && list[0].url == it.url
            }
        }
        return false
    }

    private fun updateBookmarkState(session: Session) {
        if (findBookmarkJob?.isActive == true) findBookmarkJob?.cancel()
        findBookmarkJob = launch {
            val found = findBookmarkedURL(session)
            launch(Main) {
                getManagedEmitter<QuickActionChange>()
                    .onNext(QuickActionChange.BookmarkedStateChange(found))
            }
        }
    }

    private fun shareUrl(url: String) {
        val directions = BrowserFragmentDirections.actionBrowserFragmentToShareFragment(url)
        Navigation.findNavController(view!!).navigate(directions)
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
