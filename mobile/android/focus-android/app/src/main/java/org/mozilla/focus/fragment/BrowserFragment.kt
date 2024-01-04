/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment

import android.annotation.SuppressLint
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.accessibility.AccessibilityManager
import android.webkit.MimeTypeMap
import android.widget.Toast
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.content.pm.ShortcutManagerCompat
import androidx.core.net.toUri
import androidx.core.view.isVisible
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.preference.PreferenceManager
import com.google.android.material.snackbar.Snackbar
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.mapNotNull
import kotlinx.coroutines.launch
import kotlinx.coroutines.plus
import mozilla.components.browser.state.selector.findTabOrCustomTab
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.CustomTabConfig
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.createTab
import mozilla.components.concept.engine.HitResult
import mozilla.components.feature.app.links.AppLinksFeature
import mozilla.components.feature.contextmenu.ContextMenuFeature
import mozilla.components.feature.downloads.AbstractFetchDownloadService
import mozilla.components.feature.downloads.DownloadsFeature
import mozilla.components.feature.downloads.manager.FetchDownloadManager
import mozilla.components.feature.downloads.temporary.ShareDownloadFeature
import mozilla.components.feature.media.fullscreen.MediaSessionFullscreenFeature
import mozilla.components.feature.prompts.PromptFeature
import mozilla.components.feature.session.PictureInPictureFeature
import mozilla.components.feature.session.SessionFeature
import mozilla.components.feature.sitepermissions.SitePermissionsFeature
import mozilla.components.feature.tabs.WindowFeature
import mozilla.components.feature.top.sites.TopSitesConfig
import mozilla.components.feature.top.sites.TopSitesFeature
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.service.glean.private.NoExtras
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.ktx.android.view.exitImmersiveMode
import mozilla.components.support.locale.ActivityContextWrapper
import mozilla.components.support.utils.Browsers
import mozilla.components.support.utils.StatusBarUtils
import mozilla.components.support.utils.ext.requestInPlacePermissions
import org.mozilla.focus.GleanMetrics.Browser
import org.mozilla.focus.GleanMetrics.CookieBanner
import org.mozilla.focus.GleanMetrics.Downloads
import org.mozilla.focus.GleanMetrics.OpenWith
import org.mozilla.focus.GleanMetrics.TabCount
import org.mozilla.focus.GleanMetrics.TrackingProtection
import org.mozilla.focus.R
import org.mozilla.focus.activity.InstallFirefoxActivity
import org.mozilla.focus.activity.MainActivity
import org.mozilla.focus.browser.integration.BrowserMenuController
import org.mozilla.focus.browser.integration.BrowserToolbarIntegration
import org.mozilla.focus.browser.integration.FindInPageIntegration
import org.mozilla.focus.browser.integration.FullScreenIntegration
import org.mozilla.focus.contextmenu.ContextMenuCandidates
import org.mozilla.focus.cookiebannerreducer.CookieBannerReducerAction
import org.mozilla.focus.cookiebannerreducer.CookieBannerReducerDetailsPanel
import org.mozilla.focus.cookiebannerreducer.CookieBannerReducerMiddleware
import org.mozilla.focus.cookiebannerreducer.CookieBannerReducerState
import org.mozilla.focus.cookiebannerreducer.CookieBannerReducerStore
import org.mozilla.focus.cookiebannerreducer.DefaultCookieBannerReducerInteractor
import org.mozilla.focus.databinding.FragmentBrowserBinding
import org.mozilla.focus.downloads.DownloadService
import org.mozilla.focus.engine.EngineSharedPreferencesListener
import org.mozilla.focus.ext.accessibilityManager
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.disableDynamicBehavior
import org.mozilla.focus.ext.enableDynamicBehavior
import org.mozilla.focus.ext.ifCustomTab
import org.mozilla.focus.ext.isCustomTab
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.ext.settings
import org.mozilla.focus.ext.showAsFixed
import org.mozilla.focus.ext.titleOrDomain
import org.mozilla.focus.menu.browser.DefaultBrowserMenu
import org.mozilla.focus.open.OpenWithFragment
import org.mozilla.focus.session.ui.TabsPopup
import org.mozilla.focus.settings.permissions.permissionoptions.SitePermissionOptionsStorage
import org.mozilla.focus.settings.privacy.ConnectionDetailsPanel
import org.mozilla.focus.settings.privacy.TrackingProtectionPanel
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.topsites.DefaultTopSitesStorage.Companion.TOP_SITES_MAX_LIMIT
import org.mozilla.focus.topsites.DefaultTopSitesView
import org.mozilla.focus.utils.FocusSnackbar
import org.mozilla.focus.utils.FocusSnackbarDelegate
import org.mozilla.focus.utils.IntentUtils
import org.mozilla.focus.utils.ViewUtils
import java.net.URLEncoder

/**
 * Fragment for displaying the browser UI.
 */
@Suppress("LargeClass", "TooManyFunctions")
class BrowserFragment :
    BaseFragment(),
    UserInteractionHandler,
    AccessibilityManager.AccessibilityStateChangeListener {

    private var _binding: FragmentBrowserBinding? = null
    private val binding get() = _binding!!

    private val findInPageIntegration = ViewBoundFeatureWrapper<FindInPageIntegration>()
    private val fullScreenIntegration = ViewBoundFeatureWrapper<FullScreenIntegration>()
    private var pictureInPictureFeature: PictureInPictureFeature? = null

    internal val sessionFeature = ViewBoundFeatureWrapper<SessionFeature>()
    private val promptFeature = ViewBoundFeatureWrapper<PromptFeature>()
    private val contextMenuFeature = ViewBoundFeatureWrapper<ContextMenuFeature>()
    private val downloadsFeature = ViewBoundFeatureWrapper<DownloadsFeature>()
    private val shareDownloadFeature = ViewBoundFeatureWrapper<ShareDownloadFeature>()
    private val windowFeature = ViewBoundFeatureWrapper<WindowFeature>()
    private val appLinksFeature = ViewBoundFeatureWrapper<AppLinksFeature>()
    private val topSitesFeature = ViewBoundFeatureWrapper<TopSitesFeature>()
    private var sitePermissionsFeature = ViewBoundFeatureWrapper<SitePermissionsFeature>()
    private var fullScreenMediaSessionFeature =
        ViewBoundFeatureWrapper<MediaSessionFullscreenFeature>()

    private val toolbarIntegration = ViewBoundFeatureWrapper<BrowserToolbarIntegration>()

    private var trackingProtectionPanel: TrackingProtectionPanel? = null
    private lateinit var requestPermissionLauncher: ActivityResultLauncher<Array<String>>
    private lateinit var cookieBannerReducerStore: CookieBannerReducerStore
    private lateinit var defaultCookieBannerInteractor: DefaultCookieBannerReducerInteractor
    private var tabsPopup: TabsPopup? = null
    private var siteNotSupportedSnackBarScope: CoroutineScope? = null

    /**
     * The ID of the tab associated with this fragment.
     */
    private val tabId: String
        get() = requireArguments().getString(ARGUMENT_SESSION_UUID)
            ?: throw IllegalAccessError("No session ID set on fragment")

    /**
     * The tab associated with this fragment.
     */
    val tab: SessionState
        get() = requireComponents.store.state.findTabOrCustomTab(tabId)
            // Workaround for tab not existing temporarily.
            ?: createTab("about:blank")

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        requestPermissionLauncher =
            registerForActivityResult(
                ActivityResultContracts.RequestMultiplePermissions(),
            ) { permissionsResult ->
                val grandResults = ArrayList<Int>()
                permissionsResult.entries.forEach {
                    val isGranted = it.value
                    if (isGranted) {
                        grandResults.add(PackageManager.PERMISSION_GRANTED)
                    } else {
                        grandResults.add(PackageManager.PERMISSION_DENIED)
                    }
                }
                val feature = sitePermissionsFeature.get()
                feature?.onPermissionsResult(
                    permissionsResult.keys.toTypedArray(),
                    grandResults.toIntArray(),
                )
            }
    }

    /**
     * Initialize CookieBannerStore ,Interactor and report site snackBar
     * when tacking protection panel is shown.
     */
    fun initCookieBanner() {
        cookieBannerReducerStore = CookieBannerReducerStore(
            CookieBannerReducerState(),
            listOf(
                CookieBannerReducerMiddleware(
                    ioScope = this.lifecycleScope + Dispatchers.IO,
                    cookieBannersStorage = requireContext().components.cookieBannerStorage,
                    appContext = requireContext(),
                    currentTab = tab,
                ),
            ),
        )
        defaultCookieBannerInteractor = DefaultCookieBannerReducerInteractor(
            store = cookieBannerReducerStore,
        )
        updateCookieBannerSiteToReportSnackBar()
    }

    private fun updateCookieBannerSiteToReportSnackBar() {
        siteNotSupportedSnackBarScope = cookieBannerReducerStore.flowScoped { flow ->
            flow.mapNotNull { state -> state.showSnackBarForSiteToReport }
                .distinctUntilChanged()
                .collect { showSnackBarForSiteToReport ->
                    if (showSnackBarForSiteToReport) {
                        ViewUtils.showBrandedSnackbar(
                            view,
                            R.string.cookie_banner_report_a_site_snackbar_label,
                            0,
                        )
                        cookieBannerReducerStore.dispatch(
                            CookieBannerReducerAction.ShowSnackBarForSiteToReport(
                                false,
                            ),
                        )
                    }
                }
        }
    }

    @Suppress("LongMethod", "ComplexMethod")
    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        _binding = FragmentBrowserBinding.inflate(inflater, container, false)

        requireContext().accessibilityManager.addAccessibilityStateChangeListener(this)

        val originalContext = ActivityContextWrapper.getOriginalContext(requireActivity())
        binding.engineView.setActivityContext(originalContext)

        return binding.root
    }

    @SuppressLint("VisibleForTests")
    @Suppress("ComplexCondition", "LongMethod")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        val components = requireComponents

        findInPageIntegration.set(
            FindInPageIntegration(
                components.store,
                binding.findInPage,
                binding.browserToolbar,
                binding.engineView,
            ),
            this,
            view,
        )

        fullScreenIntegration.set(
            FullScreenIntegration(
                requireActivity(),
                components.store,
                tab.id,
                components.sessionUseCases,
                requireContext().settings,
                binding.browserToolbar,
                binding.statusBarBackground,
                binding.engineView,
                parentFragmentManager,
            ),
            this,
            view,
        )

        pictureInPictureFeature = PictureInPictureFeature(
            store = components.store,
            activity = requireActivity(),
            crashReporting = components.crashReporter,
            tabId = tabId,
        )

        contextMenuFeature.set(
            ContextMenuFeature(
                parentFragmentManager,
                components.store,
                ContextMenuCandidates.get(
                    requireContext(),
                    components.tabsUseCases,
                    components.contextMenuUseCases,
                    components.appLinksUseCases,
                    view,
                    FocusSnackbarDelegate(view),
                    tab.isCustomTab(),
                ),
                binding.engineView,
                requireComponents.contextMenuUseCases,
                tabId,
                additionalNote = { hitResult -> getAdditionalNote(hitResult) },
            ),
            this,
            view,
        )

        sessionFeature.set(
            SessionFeature(
                components.store,
                components.sessionUseCases.goBack,
                binding.engineView,
                tab.id,
            ),
            this,
            view,
        )

        promptFeature.set(
            PromptFeature(
                fragment = this,
                store = components.store,
                tabsUseCases = components.tabsUseCases,
                customTabId = tryGetCustomTabId(),
                fragmentManager = parentFragmentManager,
                fileUploadsDirCleaner = requireComponents.fileUploadsDirCleaner,
                onNeedToRequestPermissions = { permissions ->
                    requestInPlacePermissions(REQUEST_KEY_PROMPT_PERMISSIONS, permissions) { result ->
                        promptFeature.get()?.onPermissionsResult(
                            result.keys.toTypedArray(),
                            result.values.map {
                                when (it) {
                                    true -> PackageManager.PERMISSION_GRANTED
                                    false -> PackageManager.PERMISSION_DENIED
                                }
                            }.toIntArray(),
                        )
                    }
                },
            ),
            this,
            view,
        )

        downloadsFeature.set(
            DownloadsFeature(
                requireContext().applicationContext,
                components.store,
                components.downloadsUseCases,
                fragmentManager = childFragmentManager,
                tabId = tabId,
                downloadManager = FetchDownloadManager(
                    requireContext().applicationContext,
                    components.store,
                    DownloadService::class,
                    notificationsDelegate = components.notificationsDelegate,
                ),
                onNeedToRequestPermissions = { permissions ->
                    requestInPlacePermissions(REQUEST_KEY_DOWNLOAD_PERMISSIONS, permissions) { result ->
                        downloadsFeature.get()?.onPermissionsResult(
                            result.keys.toTypedArray(),
                            result.values.map {
                                when (it) {
                                    true -> PackageManager.PERMISSION_GRANTED
                                    false -> PackageManager.PERMISSION_DENIED
                                }
                            }.toIntArray(),
                        )
                    }
                },
                onDownloadStopped = { state, _, status ->
                    handleDownloadStopped(state, status)
                },
            ),
            this,
            view,
        )

        shareDownloadFeature.set(
            ShareDownloadFeature(
                context = requireContext().applicationContext,
                httpClient = components.client,
                store = components.store,
                tabId = tab.id,
            ),
            this,
            view,
        )

        appLinksFeature.set(
            feature = AppLinksFeature(
                requireContext(),
                store = components.store,
                sessionId = tabId,
                fragmentManager = parentFragmentManager,
                launchInApp = { requireContext().settings.openLinksInExternalApp },
                loadUrlUseCase = requireContext().components.sessionUseCases.loadUrl,
                failedToLaunchAction = { fallbackUrl ->
                    fallbackUrl?.let {
                        val appLinksUseCases = components.appLinksUseCases
                        val getRedirect = appLinksUseCases.appLinkRedirect
                        val redirect = getRedirect.invoke(fallbackUrl)
                        redirect.appIntent?.flags = Intent.FLAG_ACTIVITY_NEW_TASK
                        appLinksUseCases.openAppLink.invoke(redirect.appIntent)
                    }
                },
            ),
            owner = this,
            view = view,
        )

        topSitesFeature.set(
            feature = TopSitesFeature(
                view = DefaultTopSitesView(requireComponents.appStore),
                storage = requireComponents.topSitesStorage,
                config = {
                    TopSitesConfig(
                        totalSites = TOP_SITES_MAX_LIMIT,
                        frecencyConfig = null,
                        providerConfig = null,
                    )
                },
            ),
            owner = this,
            view = view,
        )

        customizeToolbar()

        val customTabConfig = tab.ifCustomTab()?.config
        if (customTabConfig != null) {
            initialiseCustomTabUi(customTabConfig)

            // TODO Add custom tabs window feature support
            // We to add support for Custom Tabs here, however in order to send the window request
            // back to us through the intent system, we need to register a unique schema that we
            // can handle. For example, Fenix Nighlyt does this today with `fenix-nightly://`.
        } else {
            initialiseNormalBrowserUi()

            windowFeature.set(
                feature = WindowFeature(
                    store = components.store,
                    tabsUseCases = components.tabsUseCases,
                ),
                owner = this,
                view = view,
            )
        }

        // Feature that handles MediaSession state changes
        fullScreenMediaSessionFeature.set(
            feature = MediaSessionFullscreenFeature(requireActivity(), requireComponents.store, tryGetCustomTabId()),
            owner = this,
            view = view,
        )

        setSitePermissions(view)
    }

    private fun setSitePermissions(rootView: View) {
        sitePermissionsFeature.set(
            feature = SitePermissionsFeature(
                context = requireContext(),
                fragmentManager = parentFragmentManager,
                onNeedToRequestPermissions = { permissions ->
                    if (SitePermissionOptionsStorage(requireContext()).isSitePermissionNotBlocked(permissions)) {
                        requestPermissionLauncher.launch(permissions)
                    }
                },
                onShouldShowRequestPermissionRationale = {
                    // Since we don't request permissions this it will not be called
                    false
                },
                sitePermissionsRules = SitePermissionOptionsStorage(requireContext()).getSitePermissionsSettingsRules(),
                sessionId = tabId,
                store = requireComponents.store,
                shouldShowDoNotAskAgainCheckBox = false,
            ),
            owner = this,
            view = rootView,
        )
        if (requireComponents.appStore.state.sitePermissionOptionChange) {
            requireComponents.sessionUseCases.reload(tabId)
            requireComponents.appStore.dispatch(AppAction.SitePermissionOptionChange(false))
        }
    }

    override fun onAccessibilityStateChanged(enabled: Boolean) {
        when (enabled) {
            // using _binding, because this might be called before onCreateView.
            false -> _binding?.browserToolbar?.enableDynamicBehavior(
                requireContext(),
                binding.engineView,
            )
            true -> {
                _binding?.browserToolbar?.let {
                    it.disableDynamicBehavior(binding.engineView)
                    it.showAsFixed(requireContext(), binding.engineView)
                }
            }
        }
    }

    override fun onPictureInPictureModeChanged(enabled: Boolean) {
        pictureInPictureFeature?.onPictureInPictureModeChanged(enabled)
        if (lifecycle.currentState == Lifecycle.State.CREATED) {
            onBackPressed()
        }
    }

    private fun getAdditionalNote(hitResult: HitResult): String? {
        return if ((hitResult is HitResult.IMAGE_SRC || hitResult is HitResult.IMAGE) &&
            hitResult.src.isNotEmpty()
        ) {
            getString(R.string.contextmenu_erased_images_note2, getString(R.string.app_name))
        } else {
            null
        }
    }

    private fun customizeToolbar() {
        val controller = BrowserMenuController(
            requireComponents.sessionUseCases,
            requireComponents.appStore,
            requireComponents.store,
            requireComponents.topSitesUseCases,
            tabId,
            ::shareCurrentUrl,
            ::setShouldRequestDesktop,
            ::showAddToHomescreenDialog,
            ::showFindInPageBar,
            ::openSelectBrowser,
            ::openInBrowser,
            ::showShortcutAddedSnackBar,
        )

        if (tab.ifCustomTab()?.config == null) {
            val browserMenu = DefaultBrowserMenu(
                context = requireContext(),
                appStore = requireComponents.appStore,
                store = requireComponents.store,
                isPinningSupported = ShortcutManagerCompat.isRequestPinShortcutSupported(
                    requireContext(),
                ),
                onItemTapped = { controller.handleMenuInteraction(it) },
            )
            binding.browserToolbar.display.menuBuilder = browserMenu.menuBuilder
        }

        toolbarIntegration.set(
            BrowserToolbarIntegration(
                requireComponents.store,
                toolbar = binding.browserToolbar,
                fragment = this,
                controller = controller,
                customTabId = tryGetCustomTabId(),
                customTabsUseCases = requireComponents.customTabsUseCases,
                sessionUseCases = requireComponents.sessionUseCases,
                onUrlLongClicked = ::onUrlLongClicked,
                eraseActionListener = { erase(shouldEraseAllTabs = true) },
                tabCounterListener = ::tabCounterListener,
            ),
            owner = this,
            view = binding.browserToolbar,
        )
    }

    private fun showShortcutAddedSnackBar() {
        FocusSnackbar.make(requireView())
            .setText(requireContext().getString(R.string.snackbar_added_to_shortcuts))
            .show()
    }

    private fun initialiseNormalBrowserUi() {
        if (!requireContext().settings.isAccessibilityEnabled()) {
            binding.browserToolbar.enableDynamicBehavior(requireContext(), binding.engineView)
        } else {
            binding.browserToolbar.showAsFixed(requireContext(), binding.engineView)
        }
    }

    private fun initialiseCustomTabUi(customTabConfig: CustomTabConfig) {
        if (customTabConfig.enableUrlbarHiding && !requireContext().settings.isAccessibilityEnabled()) {
            binding.browserToolbar.enableDynamicBehavior(requireContext(), binding.engineView)
        } else {
            binding.browserToolbar.showAsFixed(requireContext(), binding.engineView)
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()

        requireContext().accessibilityManager.removeAccessibilityStateChangeListener(this)
        binding.engineView.setActivityContext(null)

        _binding = null
    }

    override fun onDestroy() {
        super.onDestroy()

        // This fragment might get destroyed before the user left immersive mode (e.g. by opening another URL from an
        // app). In this case let's leave immersive mode now when the fragment gets destroyed.
        requireActivity().exitImmersiveMode()
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        promptFeature.withFeature { it.onActivityResult(requestCode, data, resultCode) }
    }

    @OptIn(DelicateCoroutinesApi::class)
    private fun showCrashReporter(crash: Crash) {
        val fragmentManager = requireActivity().supportFragmentManager

        if (crashReporterIsVisible()) {
            // We are already displaying the crash reporter
            // No need to show another one.
            return
        }

        val crashReporterFragment = CrashReporterFragment.create()

        crashReporterFragment.onCloseTabPressed = { sendCrashReport ->
            if (sendCrashReport) {
                val crashReporter = requireComponents.crashReporter

                GlobalScope.launch(Dispatchers.IO) { crashReporter.submitReport(crash) }
            }

            requireComponents.sessionUseCases.crashRecovery.invoke()
            erase()
            hideCrashReporter()
        }

        fragmentManager
            .beginTransaction()
            .addToBackStack(null)
            .add(R.id.crash_container, crashReporterFragment, CrashReporterFragment.FRAGMENT_TAG)
            .commit()

        binding.crashContainer.isVisible = true
    }

    private fun hideCrashReporter() {
        val fragmentManager = requireActivity().supportFragmentManager
        val fragment = fragmentManager.findFragmentByTag(CrashReporterFragment.FRAGMENT_TAG)
            ?: return

        fragmentManager
            .beginTransaction()
            .remove(fragment)
            .commit()

        binding.crashContainer.isVisible = false
    }

    fun crashReporterIsVisible(): Boolean = requireActivity().supportFragmentManager.let {
        it.findFragmentByTag(CrashReporterFragment.FRAGMENT_TAG)?.isVisible ?: false
    }

    private fun handleDownloadStopped(
        state: DownloadState,
        status: DownloadState.Status,
    ) {
        val extension =
            MimeTypeMap.getFileExtensionFromUrl(URLEncoder.encode(state.filePath, "utf-8"))

        when (status) {
            DownloadState.Status.FAILED -> {
                Downloads.downloadFailed.record(Downloads.DownloadFailedExtra(extension))
            }

            DownloadState.Status.PAUSED -> {
                Downloads.downloadPaused.record(NoExtras())
            }

            DownloadState.Status.COMPLETED -> {
                Downloads.downloadCompleted.record(NoExtras())

                showDownloadCompletedSnackbar(state, extension)
            }

            else -> {
            }
        }
    }

    override fun onPause() {
        super.onPause()
        // Custom tab content should always be visible, even if the app is locked.
        if (tab.isCustomTab()) {
            view?.isVisible = true
        }
    }
    private fun showDownloadCompletedSnackbar(
        state: DownloadState,
        extension: String?,
    ) {
        val snackbar = FocusSnackbar.make(
            requireView(),
            Snackbar.LENGTH_LONG,
        )

        snackbar.setText(
            String.format(
                requireContext().getString(R.string.download_snackbar_finished),
                state.fileName,
            ),
        )

        snackbar.setAction(getString(R.string.download_snackbar_open)) { context ->
            val opened = AbstractFetchDownloadService.openFile(
                applicationContext = context.applicationContext,
                download = state,
            )

            if (!opened) {
                Toast.makeText(
                    context,
                    getString(
                        mozilla.components.feature.downloads.R.string.mozac_feature_downloads_open_not_supported1,
                        extension,
                    ),
                    Toast.LENGTH_LONG,
                ).show()
            }

            Downloads.openButtonTapped.record(
                Downloads.OpenButtonTappedExtra(fileExtension = extension, openSuccessful = opened),
            )
        }

        snackbar.show()
    }

    private fun showAddToHomescreenDialog() {
        val fragmentManager = childFragmentManager

        if (fragmentManager.findFragmentByTag(AddToHomescreenDialogFragment.FRAGMENT_TAG) != null) {
            // We are already displaying a homescreen dialog fragment (Probably a restored fragment).
            // No need to show another one.
            return
        }

        val requestDesktop = tab.content.desktopMode

        val addToHomescreenDialogFragment = AddToHomescreenDialogFragment.newInstance(
            url = tab.content.url,
            title = tab.content.titleOrDomain,
            blockingEnabled = !tab.trackingProtection.ignoredOnTrackingProtection,
            requestDesktop = requestDesktop,
        )

        try {
            addToHomescreenDialogFragment.show(
                fragmentManager,
                AddToHomescreenDialogFragment.FRAGMENT_TAG,
            )
        } catch (e: IllegalStateException) {
            // It can happen that at this point in time the activity is already in the background
            // and onSaveInstanceState() has already been called. Fragment transactions are not
            // allowed after that anymore. It's probably safe to guess that the user might not
            // be interested in adding to homescreen now.
        }
    }

    override fun onResume() {
        super.onResume()

        updateEngineColorScheme()

        // Hide status bar background if the parent activity can be casted to MainActivity
        (requireActivity() as? MainActivity)?.hideStatusBarBackground()
        StatusBarUtils.getStatusBarHeight(binding.statusBarBackground) { statusBarHeight ->
            binding.statusBarBackground.layoutParams.height = statusBarHeight
        }

        // Custom tab content should always be visible, even if the app is locked.
        if (tab.isCustomTab()) {
            view?.isVisible = true
        }

        context?.settings?.openLinksInExternalApp?.let { openLinksInExternalApp ->
            val isCustomTab = tab.isCustomTab()
            components?.appLinksInterceptor?.updateLaunchInApp {
                openLinksInExternalApp || isCustomTab
            }
        }
    }

    private fun updateEngineColorScheme() {
        val preferredColorScheme = requireComponents.settings.getPreferredColorScheme()
        if (requireComponents.engine.settings.preferredColorScheme != preferredColorScheme) {
            requireComponents.engine.settings.preferredColorScheme = preferredColorScheme
            requireComponents.sessionUseCases.reload()
        }
    }

    override fun onStop() {
        super.onStop()
        tabsPopup?.dismiss()
        trackingProtectionPanel?.hide()
        siteNotSupportedSnackBarScope?.cancel()
    }

    override fun onHomePressed() = pictureInPictureFeature?.onHomePressed() ?: false

    @Suppress("ComplexMethod", "ReturnCount")
    override fun onBackPressed(): Boolean {
        if (findInPageIntegration.onBackPressed()) {
            return true
        } else if (fullScreenIntegration.onBackPressed()) {
            return true
        } else if (sessionFeature.get()?.onBackPressed() == true) {
            return true
        } else if (tab.source is SessionState.Source.Internal.TextSelection) {
            erase()
            return true
        } else {
            if (tab.source is SessionState.Source.External || tab.isCustomTab()) {
                Browser.backButtonPressed.record(
                    Browser.BackButtonPressedExtra("erase_to_external_app"),
                )

                // This session has been started from a VIEW intent. Go back to the previous app
                // immediately and erase the current browsing session.
                erase()

                // If there are no other sessions then we remove the whole task because otherwise
                // the old session might still be partially visible in the app switcher.
                if (requireComponents.store.state.privateTabs.isEmpty()) {
                    requireActivity().finishAndRemoveTask()
                } else {
                    requireActivity().finish()
                }
                // We can't show a snackbar outside of the app. So let's show a toast instead.
                Toast.makeText(context, R.string.feedback_erase_custom_tab, Toast.LENGTH_SHORT).show()
            } else {
                // Just go back to the home screen.
                Browser.backButtonPressed.record(
                    Browser.BackButtonPressedExtra("erase_to_home"),
                )

                erase()
            }
        }

        return true
    }

    fun erase(shouldEraseAllTabs: Boolean = false) {
        if (shouldEraseAllTabs) {
            requireComponents.appStore.dispatch(AppAction.NavigateUp(null))
            requireComponents.tabsUseCases.removeAllTabs()
        } else {
            requireComponents.tabsUseCases.removeTab(tab.id)
            requireComponents.appStore.dispatch(AppAction.NavigateUp(requireComponents.store.state.selectedTabId))
        }
    }

    private fun shareCurrentUrl() {
        val shareIntent = Intent(Intent.ACTION_SEND)
        shareIntent.type = "text/plain"
        shareIntent.putExtra(Intent.EXTRA_TEXT, tab.content.url)

        val title = tab.content.title
        if (title.isNotEmpty()) {
            shareIntent.putExtra(Intent.EXTRA_SUBJECT, title)
        }
        startActivity(
            IntentUtils.getIntentChooser(
                context = requireContext(),
                intent = shareIntent,
                chooserTitle = getString(R.string.share_dialog_title),
            ),
        )
    }

    private fun openInBrowser() {
        // Release the session from this view so that it can immediately be rendered by a different view
        sessionFeature.get()?.release()

        requireComponents.customTabsUseCases.migrate(tab.id)

        val intent = Intent(context, MainActivity::class.java)
        intent.action = Intent.ACTION_MAIN
        intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP
        startActivity(intent)

        // Close this activity (and the task) since it is no longer displaying any session
        val activity = activity
        activity?.finishAndRemoveTask()
    }

    internal fun edit() {
        requireComponents.appStore.dispatch(
            AppAction.EditAction(tab.id),
        )
    }

    private fun tabCounterListener() {
        val openedTabs = requireComponents.store.state.tabs.size

        tabsPopup = TabsPopup(binding.browserToolbar, requireComponents).also { currentTabsPopup ->
            currentTabsPopup.showAsDropDown(
                binding.browserToolbar,
                0,
                0,
                Gravity.END,
            )
        }

        TabCount.sessionButtonTapped.record(TabCount.SessionButtonTappedExtra(openedTabs))
    }

    private fun showFindInPageBar() {
        findInPageIntegration.get()?.show(tab)
    }

    private fun openSelectBrowser() {
        val browsers = Browsers.forUrl(requireContext(), tab.content.url)

        val apps = browsers.installedBrowsers.filterNot { it.packageName == requireContext().packageName }
        val store = if (browsers.hasFirefoxBrandedBrowserInstalled) {
            null
        } else {
            InstallFirefoxActivity.resolveAppStore(requireContext())
        }

        val fragment = OpenWithFragment.newInstance(
            apps.toTypedArray(),
            tab.content.url,
            store,
        )
        @Suppress("DEPRECATION")
        fragment.show(requireFragmentManager(), OpenWithFragment.FRAGMENT_TAG)

        OpenWith.listDisplayed.record(OpenWith.ListDisplayedExtra(apps.size))
    }

    internal fun closeCustomTab() {
        requireComponents.customTabsUseCases.remove(tab.id)
        requireActivity().finish()
    }

    private fun setShouldRequestDesktop(enabled: Boolean) {
        if (enabled) {
            PreferenceManager.getDefaultSharedPreferences(requireContext()).edit()
                .putBoolean(
                    requireContext().getString(R.string.has_requested_desktop),
                    true,
                ).apply()
        }
        requireComponents.sessionUseCases.requestDesktopSite(enabled, tab.id)
    }

    fun showTrackingProtectionPanel() {
        trackingProtectionPanel = TrackingProtectionPanel(
            context = requireContext(),
            lifecycleOwner = this,
            cookieBannerReducerStore = cookieBannerReducerStore,
            tabUrl = tab.content.url,
            isTrackingProtectionOn = tab.trackingProtection.ignoredOnTrackingProtection.not(),
            isConnectionSecure = tab.content.securityInfo.secure,
            blockedTrackersCount = requireContext().settings
                .getTotalBlockedTrackersCount(),
            toggleTrackingProtection = ::toggleTrackingProtection,
            updateTrackingProtectionPolicy = { tracker, isEnabled ->
                EngineSharedPreferencesListener(requireContext())
                    .updateTrackingProtectionPolicy(
                        source = EngineSharedPreferencesListener.ChangeSource.PANEL.source,
                        tracker = tracker,
                        isEnabled = isEnabled,
                    )
                reloadCurrentTab()
            },
            showConnectionInfo = ::showConnectionInfo,
            showCookieBannerExceptionsDetailsPanel = ::showCookieBannerExceptionDetailsPanel,
        ).also { currentEtp -> context?.let { currentEtp.show() } }
    }

    private fun reloadCurrentTab() {
        requireComponents.sessionUseCases.reload(tab.id)
    }

    private fun showCookieBannerExceptionDetailsPanel() {
        val cookieBannerExceptionDetailsPanel = CookieBannerReducerDetailsPanel(
            context = requireContext(),
            cookieBannerReducerStore = cookieBannerReducerStore,
            ioScope = viewLifecycleOwner.lifecycleScope + Dispatchers.IO,
            tabUrl = tab.content.url,
            goBack = { trackingProtectionPanel?.show() },
            defaultCookieBannerInteractor = defaultCookieBannerInteractor,
        )
        trackingProtectionPanel?.hide()
        cookieBannerExceptionDetailsPanel.show()
        CookieBanner.visitedPanel.record(NoExtras())
    }

    private fun showConnectionInfo() {
        val connectionInfoPanel = ConnectionDetailsPanel(
            context = requireContext(),
            tabTitle = tab.content.title,
            tabUrl = tab.content.url,
            isConnectionSecure = tab.content.securityInfo.secure,
            goBack = { trackingProtectionPanel?.show() },
        )
        trackingProtectionPanel?.hide()
        connectionInfoPanel.show()
    }

    private fun toggleTrackingProtection(enable: Boolean) {
        with(requireComponents) {
            if (enable) {
                trackingProtectionUseCases.removeException(tab.id)
            } else {
                trackingProtectionUseCases.addException(tab.id, persistInPrivateMode = true)
            }
        }

        reloadCurrentTab()

        TrackingProtection.hasEverChangedEtp.set(true)
        TrackingProtection.trackingProtectionChanged.record(
            TrackingProtection.TrackingProtectionChangedExtra(
                isEnabled = enable,
            ),
        )
    }

    private fun onUrlLongClicked(): Boolean {
        val context = activity ?: return false

        return if (tab.isCustomTab()) {
            val clipBoard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
            val uri = tab.content.url.toUri()
            clipBoard.setPrimaryClip(ClipData.newRawUri("Uri", uri))
            Toast.makeText(context, getString(R.string.custom_tab_copy_url_action), Toast.LENGTH_SHORT).show()
            true
        } else {
            false
        }
    }

    private fun tryGetCustomTabId() = if (tab.isCustomTab()) {
        tab.id
    } else {
        null
    }

    fun handleTabCrash(crash: Crash) {
        showCrashReporter(crash)
    }

    companion object {
        const val FRAGMENT_TAG = "browser"

        private const val ARGUMENT_SESSION_UUID = "sessionUUID"

        private const val REQUEST_KEY_DOWNLOAD_PERMISSIONS = "downloadFeature"
        private const val REQUEST_KEY_PROMPT_PERMISSIONS = "promptFeature"
        fun createForTab(tabId: String): BrowserFragment {
            val fragment = BrowserFragment()
            fragment.arguments = Bundle().apply {
                putString(ARGUMENT_SESSION_UUID, tabId)
            }
            return fragment
        }
    }
}
