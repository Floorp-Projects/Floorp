/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.content.Intent
import android.content.pm.ActivityInfo
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.CallSuper
import androidx.appcompat.app.AppCompatActivity
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.net.toUri
import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModelProviders
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import com.google.android.material.snackbar.Snackbar
import kotlinx.android.synthetic.main.component_search.*
import kotlinx.android.synthetic.main.fragment_browser.*
import kotlinx.android.synthetic.main.fragment_browser.view.*
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.intent.EXTRA_SESSION_ID
import mozilla.components.feature.app.links.AppLinksFeature
import mozilla.components.feature.contextmenu.ContextMenuFeature
import mozilla.components.feature.downloads.DownloadsFeature
import mozilla.components.feature.downloads.manager.FetchDownloadManager
import mozilla.components.feature.prompts.PromptFeature
import mozilla.components.feature.session.FullScreenFeature
import mozilla.components.feature.session.SessionFeature
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.session.SwipeRefreshFeature
import mozilla.components.feature.sitepermissions.SitePermissions
import mozilla.components.feature.sitepermissions.SitePermissionsFeature
import mozilla.components.feature.sitepermissions.SitePermissionsRules
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.base.feature.PermissionsFeature
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.ktx.android.view.exitImmersiveModeIfNeeded
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ThemeManager
import org.mozilla.fenix.collections.CreateCollectionViewModel
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.FindInPageIntegration
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.toolbar.BrowserInteractor
import org.mozilla.fenix.components.toolbar.BrowserState
import org.mozilla.fenix.components.toolbar.BrowserStore
import org.mozilla.fenix.components.toolbar.BrowserToolbarController
import org.mozilla.fenix.components.toolbar.BrowserToolbarView
import org.mozilla.fenix.components.toolbar.DefaultBrowserToolbarController
import org.mozilla.fenix.components.toolbar.QuickActionSheetState
import org.mozilla.fenix.components.toolbar.ToolbarIntegration
import org.mozilla.fenix.downloads.DownloadService
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.enterToImmersiveMode
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.utils.Settings

/**
 * Base fragment extended by [BrowserFragment].
 * This class only contains shared code focused on the main browsing content.
 * UI code specific to the app or to custom tabs can be found in the subclasses.
 */
@Suppress("TooManyFunctions")
abstract class BaseBrowserFragment : Fragment(), BackHandler {
    protected lateinit var browserStore: BrowserStore
    protected lateinit var browserInteractor: BrowserInteractor
    protected lateinit var browserToolbarView: BrowserToolbarView

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

    var customTabSessionId: String? = null

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
        ThemeManager.applyStatusBarTheme(activity.window, activity.themeManager, activity)

        val appLink = requireComponents.useCases.appLinksUseCases.appLinkRedirect
        browserStore = StoreProvider.get(this) {
            BrowserStore(
                BrowserState(
                    quickActionSheetState = QuickActionSheetState(
                        readable = getSessionById()?.readerable ?: false,
                        bookmarked = false,
                        readerActive = getSessionById()?.readerMode ?: false,
                        bounceNeeded = false,
                        isAppLink = getSessionById()?.let { appLink.invoke(it.url).hasExternalApp() } ?: false
                    )
                )
            )
        }

        return view
    }

    @Suppress("ComplexMethod")
    @CallSuper
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val sessionManager = requireComponents.core.sessionManager

        getSessionById()?.let { session ->
            val viewModel = activity!!.run {
                ViewModelProviders.of(this).get(CreateCollectionViewModel::class.java)
            }

            val browserToolbarController = DefaultBrowserToolbarController(
                context!!,
                findNavController(),
                findInPageLauncher = { findInPageIntegration.withFeature { it.launch() } },
                nestedScrollQuickActionView = nestedScrollQuickAction,
                engineView = engineView,
                currentSession = session,
                viewModel = viewModel
            )

            browserInteractor = createBrowserToolbarViewInteractor(browserToolbarController, session)

            browserToolbarView = BrowserToolbarView(
                container = view.browserLayout,
                interactor = browserInteractor,
                currentSession = session
            )

            toolbarIntegration.set(
                feature = browserToolbarView.toolbarIntegration,
                owner = this,
                view = view
            )

            findInPageIntegration.set(
                feature = FindInPageIntegration(
                    sessionManager = requireComponents.core.sessionManager,
                    sessionId = customTabSessionId,
                    view = view.findInPageView,
                    engineView = view.engineView,
                    toolbar = toolbar
                ),
                owner = this,
                view = view
            )

            browserToolbarView.view.setOnSiteSecurityClickedListener {
                showQuickSettingsDialog()
            }
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
                requireContext().applicationContext,
                sessionManager = sessionManager,
                fragmentManager = childFragmentManager,
                sessionId = customTabSessionId,
                downloadManager = FetchDownloadManager(requireContext().applicationContext, DownloadService::class),
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
            ) { inFullScreen ->
                if (inFullScreen) {
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
                view.swipeRefresh.apply {
                    val (topMargin, bottomMargin) = if (inFullScreen) 0 to 0 else getEngineMargins()
                    (layoutParams as CoordinatorLayout.LayoutParams).setMargins(0, topMargin, 0, bottomMargin)
                }
            },
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
    }

    @CallSuper
    override fun onResume() {
        super.onResume()
        val session = getSessionById() ?: return
        val components = requireComponents

        val preferredColorScheme = components.core.getPreferredColorScheme()
        if (components.core.engine.settings.preferredColorScheme != preferredColorScheme) {
            components.core.engine.settings.preferredColorScheme = preferredColorScheme
            components.useCases.sessionUseCases.reload()
        }
        (activity as HomeActivity).updateThemeForSession(session)
        (activity as AppCompatActivity).supportActionBar?.hide()

        assignSitePermissionsRules()
    }

    /**
     * Exits full screen mode.
     */
    final override fun onPause() {
        super.onPause()
        fullScreenFeature.onBackPressed()
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
     * Removes the session if it was opened by an ACTION_VIEW intent.
     */
    protected open fun removeSessionIfNeeded(): Boolean {
        getSessionById()?.let { session ->
            if (session.source == Session.Source.ACTION_VIEW) requireComponents.core.sessionManager.remove(session)
        }
        return false
    }

    protected abstract fun createBrowserToolbarViewInteractor(
        browserToolbarController: BrowserToolbarController,
        session: Session
    ): BrowserInteractor

    /**
     * Returns the top and bottom margins.
     */
    protected abstract fun getEngineMargins(): Pair<Int, Int>

    /**
     * Returns the layout [android.view.Gravity] for the quick settings dialog.
     */
    protected abstract fun getAppropriateLayoutGravity(): Int

    /**
     * Updates the site permissions rules based on user settings.
     */
    private fun assignSitePermissionsRules() {
        val settings = Settings.getInstance(requireContext())

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

    /**
     * Returns the current session.
     */
    protected fun getSessionById(): Session? {
        val sessionManager = context?.components?.core?.sessionManager ?: return null
        return if (customTabSessionId != null) {
            sessionManager.findSessionById(customTabSessionId!!)
        } else {
            sessionManager.selectedSession
        }
    }

    companion object {
        private const val KEY_CUSTOM_TAB_SESSION_ID = "custom_tab_session_id"
        private const val REQUEST_CODE_DOWNLOAD_PERMISSIONS = 1
        private const val REQUEST_CODE_PROMPT_PERMISSIONS = 2
        private const val REQUEST_CODE_APP_PERMISSIONS = 3
    }
}
