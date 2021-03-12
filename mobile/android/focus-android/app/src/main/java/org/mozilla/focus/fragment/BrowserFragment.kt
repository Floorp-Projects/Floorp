/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment

import android.app.PendingIntent
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.accessibility.AccessibilityEvent
import android.view.accessibility.AccessibilityManager
import android.webkit.MimeTypeMap
import android.widget.FrameLayout
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.TextView
import android.widget.Toast
import androidx.annotation.RequiresApi
import androidx.core.content.ContextCompat
import androidx.preference.PreferenceManager
import com.google.android.material.appbar.AppBarLayout
import com.google.android.material.snackbar.Snackbar
import kotlinx.android.synthetic.main.fragment_browser.*
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.findTabOrCustomTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineView
import mozilla.components.feature.contextmenu.ContextMenuCandidate
import mozilla.components.feature.contextmenu.ContextMenuFeature
import mozilla.components.feature.downloads.AbstractFetchDownloadService
import mozilla.components.feature.downloads.DownloadsFeature
import mozilla.components.feature.downloads.manager.FetchDownloadManager
import mozilla.components.feature.downloads.share.ShareDownloadFeature
import mozilla.components.feature.findinpage.view.FindInPageBar
import mozilla.components.feature.prompts.PromptFeature
import mozilla.components.feature.session.SessionFeature
import mozilla.components.lib.crash.Crash
import mozilla.components.support.base.feature.PermissionsFeature
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.utils.ColorUtils
import mozilla.components.support.utils.DrawableUtils
import org.mozilla.focus.R
import org.mozilla.focus.activity.InstallFirefoxActivity
import org.mozilla.focus.activity.MainActivity
import org.mozilla.focus.biometrics.BiometricAuthenticationDialogFragment
import org.mozilla.focus.biometrics.BiometricAuthenticationHandler
import org.mozilla.focus.biometrics.Biometrics
import org.mozilla.focus.browser.DisplayToolbar
import org.mozilla.focus.browser.binding.BlockingThemeBinding
import org.mozilla.focus.browser.binding.LoadingBinding
import org.mozilla.focus.browser.binding.MenuBinding
import org.mozilla.focus.browser.binding.ProgressBinding
import org.mozilla.focus.browser.binding.SecurityInfoBinding
import org.mozilla.focus.browser.binding.ToolbarButtonBinding
import org.mozilla.focus.browser.binding.UrlBinding
import org.mozilla.focus.browser.integration.FindInPageIntegration
import org.mozilla.focus.browser.integration.FullScreenIntegration
import org.mozilla.focus.downloads.DownloadService
import org.mozilla.focus.ext.contentState
import org.mozilla.focus.ext.isSearch
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.locale.LocaleAwareAppCompatActivity
import org.mozilla.focus.locale.LocaleAwareFragment
import org.mozilla.focus.observer.LoadTimeObserver
import org.mozilla.focus.open.OpenWithFragment
import org.mozilla.focus.popup.PopupUtils
import org.mozilla.focus.session.ui.SessionsSheetFragment
import org.mozilla.focus.telemetry.CrashReporterWrapper
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.utils.AppPermissionCodes.REQUEST_CODE_DOWNLOAD_PERMISSIONS
import org.mozilla.focus.utils.AppPermissionCodes.REQUEST_CODE_PROMPT_PERMISSIONS
import org.mozilla.focus.utils.Browsers
import org.mozilla.focus.utils.StatusBarUtils
import org.mozilla.focus.utils.SupportUtils
import org.mozilla.focus.utils.createTab
import org.mozilla.focus.widget.AnimatedProgressBar
import org.mozilla.focus.widget.FloatingEraseButton
import org.mozilla.focus.widget.FloatingSessionsButton

/**
 * Fragment for displaying the browser UI.
 */
@Suppress("LargeClass", "TooManyFunctions")
class BrowserFragment :
    LocaleAwareFragment(),
    View.OnClickListener,
    View.OnLongClickListener,
    BiometricAuthenticationDialogFragment.BiometricAuthenticationListener {

    private var urlView: TextView? = null
    private var statusBar: View? = null
    private var urlBar: View? = null
    private var popupTint: FrameLayout? = null

    private var engineView: EngineView? = null

    private val findInPageIntegration = ViewBoundFeatureWrapper<FindInPageIntegration>()
    private val fullScreenIntegration = ViewBoundFeatureWrapper<FullScreenIntegration>()

    private val sessionFeature = ViewBoundFeatureWrapper<SessionFeature>()
    private val promptFeature = ViewBoundFeatureWrapper<PromptFeature>()
    private val contextMenuFeature = ViewBoundFeatureWrapper<ContextMenuFeature>()
    private val downloadsFeature = ViewBoundFeatureWrapper<DownloadsFeature>()
    private val shareDownloadFeature = ViewBoundFeatureWrapper<ShareDownloadFeature>()

    private val urlBinding = ViewBoundFeatureWrapper<UrlBinding>()
    private val securityInfoBinding = ViewBoundFeatureWrapper<SecurityInfoBinding>()
    private val loadingBinding = ViewBoundFeatureWrapper<LoadingBinding>()
    private val progressBinding = ViewBoundFeatureWrapper<ProgressBinding>()
    private val menuBinding = ViewBoundFeatureWrapper<MenuBinding>()
    private val toolbarButtonBinding = ViewBoundFeatureWrapper<ToolbarButtonBinding>()
    private val blockingThemeBinding = ViewBoundFeatureWrapper<BlockingThemeBinding>()

    private var biometricController: BiometricAuthenticationHandler? = null

    var openedFromExternalLink: Boolean = false

    lateinit var session: Session
        private set

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val sessionUUID = arguments!!.getString(ARGUMENT_SESSION_UUID) ?: throw IllegalAccessError("No session exists")

        session = requireComponents.sessionManager.findSessionById(sessionUUID) ?: createTab("about:blank")
    }

    override fun onPause() {
        super.onPause()

        if (Biometrics.isBiometricsEnabled(requireContext())) {
            biometricController?.stopListening()
            view!!.alpha = 0f
        }

        menuBinding.get()?.pause()
    }

    @Suppress("LongMethod", "ComplexMethod")
    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        val view = inflater.inflate(R.layout.fragment_browser, container, false)

        urlBar = view.findViewById(R.id.urlbar)
        statusBar = view.findViewById(R.id.status_bar_background)

        popupTint = view.findViewById(R.id.popup_tint)

        urlView = view.findViewById<View>(R.id.display_url) as TextView
        urlView!!.setOnLongClickListener(this)

        LoadTimeObserver.addObservers(session, this)

        val blockIcon = view.findViewById<View>(R.id.block_image) as ImageView
        blockIcon.setImageResource(R.drawable.ic_tracking_protection_disabled)

        if (session.isCustomTabSession()) {
            initialiseCustomTabUi(view)
        } else {
            initialiseNormalBrowserUi(view)
        }

        return view
    }

    @Suppress("ComplexCondition", "LongMethod")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        val components = requireComponents

        engineView = (view.findViewById<View>(R.id.webview) as EngineView)

        val progressView = view.findViewById<View>(R.id.progress) as AnimatedProgressBar
        val menuView = view.findViewById<View>(R.id.menuView) as ImageButton
        val blockView = view.findViewById<View>(R.id.block) as FrameLayout
        val securityView = view.findViewById<ImageView>(R.id.security_info)
        val toolbarView = view.findViewById<DisplayToolbar>(R.id.appbar)

        findInPageIntegration.set(FindInPageIntegration(
            components.store,
            view.findViewById<FindInPageBar>(R.id.find_in_page),
            engineView!!
        ), this, view)

        fullScreenIntegration.set(FullScreenIntegration(
            requireActivity(),
            components.store,
            components.sessionManager,
            session.id,
            toolbarView!!,
            statusBar!!
        ), this, view)

        contextMenuFeature.set(ContextMenuFeature(
            parentFragmentManager,
            components.store,
            ContextMenuCandidate.defaultCandidates(
                    requireContext(),
                    components.tabsUseCases,
                    components.contextMenuUseCases,
                    view
            ) + ContextMenuCandidate.createOpenInExternalAppCandidate(
                requireContext(),
                components.appLinksUseCases
            ),
            engineView!!,
            requireComponents.contextMenuUseCases
        ), this, view)

        sessionFeature.set(SessionFeature(
            components.store,
            components.sessionUseCases.goBack,
            engineView!!,
            session.id
        ), this, view)

        promptFeature.set(PromptFeature(
            fragment = this,
            store = components.store,
            customTabId = session.id,
            fragmentManager = parentFragmentManager,
            onNeedToRequestPermissions = { permissions ->
                requestPermissions(permissions, REQUEST_CODE_PROMPT_PERMISSIONS)
            }
        ), this, view)

        downloadsFeature.set(DownloadsFeature(
            requireContext().applicationContext,
            components.store,
            components.downloadsUseCases,
                fragmentManager = childFragmentManager,
                downloadManager = FetchDownloadManager(
                        requireContext().applicationContext,
                        components.store,
                        DownloadService::class
                ),
                onNeedToRequestPermissions = { permissions ->
                    requestPermissions(permissions, REQUEST_CODE_DOWNLOAD_PERMISSIONS)
                },
                onDownloadStopped = { state, _, status ->
                    showDownloadSnackbar(state, status)
                }
        ), this, view)

        shareDownloadFeature.set(ShareDownloadFeature(
            context = requireContext().applicationContext,
            httpClient = components.client,
            store = components.store,
            tabId = session.id
        ), this, view)

        urlBinding.set(
            UrlBinding(
                components.store,
                session.id,
                urlView!!
            ),
            owner = this,
            view = urlView!!
        )

        menuBinding.set(
            MenuBinding(
                this,
                components.store,
                session.id,
                menuView
            ),
            owner = this,
            view = view
        )

        securityInfoBinding.set(
            SecurityInfoBinding(
                this,
                components.store,
                session.id,
                securityView!!
            ),
            owner = this,
            view = securityView
        )

        loadingBinding.set(
            LoadingBinding(
                components.store,
                session.id,
                progressView
            ),
            owner = this,
            view = view
        )

        progressBinding.set(
            ProgressBinding(
                components.store,
                session.id,
                progressView
            ),
            owner = this,
            view = progressView
        )

        blockingThemeBinding.set(
            BlockingThemeBinding(
                components.store,
                session,
                statusBar!!,
                urlBar!!,
                blockView
            ),
            owner = this,
            view = statusBar!!
        )

        val refreshButton: View? = view.findViewById(R.id.refresh)
        val stopButton: View? = view.findViewById(R.id.stop)
        val forwardButton: View? = view.findViewById(R.id.forward)
        val backButton: View? = view.findViewById(R.id.back)

        if (refreshButton != null && stopButton != null && forwardButton != null && backButton != null) {
            toolbarButtonBinding.set(
                ToolbarButtonBinding(
                    components.store,
                    session.id,
                    forwardButton,
                    backButton,
                    refreshButton,
                    stopButton
                ),
                owner = this,
                view = forwardButton
            )

            refreshButton.setOnClickListener(this)
            stopButton.setOnClickListener(this)
            forwardButton.setOnClickListener(this)
            backButton.setOnClickListener(this)
        }
    }

    private fun initialiseNormalBrowserUi(view: View) {
        val eraseButton = view.findViewById<FloatingEraseButton>(R.id.erase)
        eraseButton.setOnClickListener(this)

        urlView!!.setOnClickListener(this)

        val tabsButton = view.findViewById<FloatingSessionsButton>(R.id.tabs)
        tabsButton.setOnClickListener(this)

        val sessionManager = requireComponents.sessionManager
        @Suppress("DEPRECATION")
        sessionManager.register(object : SessionManager.Observer {
            override fun onSessionAdded(session: Session) {
                tabsButton.updateSessionsCount(sessionManager.sessions.size)
                eraseButton.updateSessionsCount(sessionManager.sessions.size)
            }

            override fun onSessionRemoved(session: Session) {
                tabsButton.updateSessionsCount(sessionManager.sessions.size)
                eraseButton.updateSessionsCount(sessionManager.sessions.size)
            }

            override fun onAllSessionsRemoved() {
                tabsButton.updateSessionsCount(sessionManager.sessions.size)
                eraseButton.updateSessionsCount(sessionManager.sessions.size)
            }
        })

        tabsButton.updateSessionsCount(sessionManager.sessions.size)
        eraseButton.updateSessionsCount(sessionManager.sessions.size)
    }

    private fun initialiseCustomTabUi(view: View) {
        val customTabConfig = session.customTabConfig!!

        // Unfortunately there's no simpler way to have the FAB only in normal-browser mode.
        // - ViewStub: requires splitting attributes for the FAB between the ViewStub, and actual FAB layout file.
        //             Moreover, the layout behaviour just doesn't work unless you set it programatically.
        // - View.GONE: doesn't work because the layout-behaviour makes the FAB visible again when scrolling.
        // - Adding at runtime: works, but then we need to use a separate layout file (and you need
        //   to set some attributes programatically, same as ViewStub).
        val erase = view.findViewById<FloatingEraseButton>(R.id.erase)
        val eraseContainer = erase.parent as ViewGroup
        eraseContainer.removeView(erase)

        val sessions = view.findViewById<FloatingSessionsButton>(R.id.tabs)
        eraseContainer.removeView(sessions)

        val textColor: Int

        if (customTabConfig.toolbarColor != null) {
            urlBar!!.setBackgroundColor(customTabConfig.toolbarColor!!)

            textColor = ColorUtils.getReadableTextColor(customTabConfig.toolbarColor!!)
            urlView!!.setTextColor(textColor)
        } else {
            textColor = Color.WHITE
        }

        val closeButton = view.findViewById<View>(R.id.customtab_close) as ImageView

        closeButton.visibility = View.VISIBLE
        closeButton.setOnClickListener(this)

        if (customTabConfig.closeButtonIcon != null) {
            closeButton.setImageBitmap(customTabConfig.closeButtonIcon)
        } else {
            // Always set the icon in case it's been overridden by a previous CT invocation
            val closeIcon = DrawableUtils.loadAndTintDrawable(requireContext(), R.drawable.ic_close, textColor)

            closeButton.setImageDrawable(closeIcon)
        }

        if (!customTabConfig.enableUrlbarHiding) {
            val params = urlBar!!.layoutParams as AppBarLayout.LayoutParams
            params.scrollFlags = 0
        }

        if (customTabConfig.actionButtonConfig != null) {
            val actionButton = view.findViewById<View>(R.id.customtab_actionbutton) as ImageButton
            actionButton.visibility = View.VISIBLE

            actionButton.setImageBitmap(customTabConfig.actionButtonConfig!!.icon)
            actionButton.contentDescription = customTabConfig.actionButtonConfig!!.description

            val pendingIntent = customTabConfig.actionButtonConfig!!.pendingIntent

            actionButton.setOnClickListener {
                try {
                    val intent = Intent()
                    intent.data = Uri.parse(session.url)

                    pendingIntent.send(context, 0, intent)
                } catch (e: PendingIntent.CanceledException) {
                    // There's really nothing we can do here...
                }

                TelemetryWrapper.customTabActionButtonEvent()
            }
        } else {
            // If the third-party app doesn't provide an action button configuration then we are
            // going to disable a "Share" button in the toolbar instead.

            val shareButton = view.findViewById<ImageButton>(R.id.customtab_actionbutton)
            shareButton.visibility = View.VISIBLE
            shareButton.setImageDrawable(
                DrawableUtils.loadAndTintDrawable(
                    requireContext(),
                    R.drawable.ic_share,
                    textColor
                )
            )
            shareButton.contentDescription = getString(R.string.menu_share)
            shareButton.setOnClickListener { shareCurrentUrl() }
        }

        // We need to tint some icons.. We already tinted the close button above. Let's tint our other icons too.
        securityInfoBinding.get()?.updateColorFilter(textColor)

        val menuIcon = DrawableUtils.loadAndTintDrawable(requireContext(), R.drawable.ic_menu, textColor)
        menuBinding.get()?.updateIcon(menuIcon)
    }

    override fun onDestroy() {
        super.onDestroy()

        // This fragment might get destroyed before the user left immersive mode (e.g. by opening another URL from an
        // app). In this case let's leave immersive mode now when the fragment gets destroyed.
        fullScreenIntegration.get()?.exitImmersiveModeIfNeeded()
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
        val feature: PermissionsFeature? = when (requestCode) {
            REQUEST_CODE_PROMPT_PERMISSIONS -> promptFeature.get()
            REQUEST_CODE_DOWNLOAD_PERMISSIONS -> downloadsFeature.get()
            else -> null
        }

        feature?.onPermissionsResult(permissions, grantResults)
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        promptFeature.withFeature { it.onActivityResult(requestCode, data, resultCode) }
    }

    private fun showCrashReporter(crash: Crash) {
        val fragmentManager = requireActivity().supportFragmentManager

        if (crashReporterIsVisible()) {
            // We are already displaying the crash reporter
            // No need to show another one.
            return
        }

        val crashReporterFragment = CrashReporterFragment.create()

        crashReporterFragment.onCloseTabPressed = { sendCrashReport ->
            if (sendCrashReport) { CrashReporterWrapper.submitCrash(crash) }
            erase()
            hideCrashReporter()
        }

        fragmentManager
                .beginTransaction()
                .addToBackStack(null)
                .add(R.id.crash_container, crashReporterFragment, CrashReporterFragment.FRAGMENT_TAG)
                .commit()

        crash_container.visibility = View.VISIBLE
        tabs.hide()
        erase.hide()
        securityInfoBinding.get()?.updateIcon(R.drawable.ic_firefox)
        menuBinding.get()?.hideMenuButton()
        urlView?.text = requireContext().getString(R.string.tab_crash_report_title)
    }

    private fun hideCrashReporter() {
        val fragmentManager = requireActivity().supportFragmentManager
        val fragment = fragmentManager.findFragmentByTag(CrashReporterFragment.FRAGMENT_TAG)
                ?: return

        fragmentManager
                .beginTransaction()
                .remove(fragment)
                .commit()

        crash_container.visibility = View.GONE
        tabs.show()
        erase.show()
        securityInfoBinding.get()?.updateIcon(R.drawable.ic_internet)
        menuBinding.get()?.showMenuButton()
        urlView?.text = session.let {
            val contentState = requireComponents.store.contentState(it.id)
            if (contentState?.isSearch == true) contentState.searchTerms else contentState?.url
        }
    }

    fun crashReporterIsVisible(): Boolean = requireActivity().supportFragmentManager.let {
        it.findFragmentByTag(CrashReporterFragment.FRAGMENT_TAG)?.isVisible ?: false
    }

    private fun showDownloadSnackbar(
        state: DownloadState,
        status: DownloadState.Status
    ) {
        if (status != DownloadState.Status.COMPLETED) {
            // We currently only show an in-app snackbar for completed downloads.
            return
        }

        val snackbar = Snackbar.make(
            requireView(),
            String.format(requireContext().getString(R.string.download_snackbar_finished), state.fileName),
            Snackbar.LENGTH_LONG
        )

        snackbar.setAction(getString(R.string.download_snackbar_open)) {
            val opened = AbstractFetchDownloadService.openFile(
                context = requireContext(),
                contentType = state.contentType,
                filePath = state.filePath
            )

            if (!opened) {
                val extension = MimeTypeMap.getFileExtensionFromUrl(state.filePath)

                Toast.makeText(
                    context,
                    getString(
                        mozilla.components.feature.downloads.R.string.mozac_feature_downloads_open_not_supported1,
                        extension
                    ),
                    Toast.LENGTH_LONG
                ).show()
            }
        }

        snackbar.setActionTextColor(ContextCompat.getColor(requireContext(), R.color.snackbarActionText))

        snackbar.show()
    }

    internal fun showAddToHomescreenDialog(url: String, title: String) {
        val fragmentManager = childFragmentManager

        if (fragmentManager.findFragmentByTag(AddToHomescreenDialogFragment.FRAGMENT_TAG) != null) {
            // We are already displaying a homescreen dialog fragment (Probably a restored fragment).
            // No need to show another one.
            return
        }

        val tab = requireComponents.store.state.findTabOrCustomTab(session.id)
        val requestDesktop = tab?.content?.desktopMode ?: false

        val addToHomescreenDialogFragment = AddToHomescreenDialogFragment.newInstance(
            url,
            title,
            session.trackerBlockingEnabled,
            requestDesktop = requestDesktop
        )

        try {
            addToHomescreenDialogFragment.show(
                fragmentManager,
                AddToHomescreenDialogFragment.FRAGMENT_TAG
            )
        } catch (e: IllegalStateException) {
            // It can happen that at this point in time the activity is already in the background
            // and onSaveInstanceState() has already been called. Fragment transactions are not
            // allowed after that anymore. It's probably safe to guess that the user might not
            // be interested in adding to homescreen now.
        }
    }

    override fun biometricCreateNewSessionWithLink() {
        for (session in requireComponents.sessionManager.sessions) {
            if (session != requireComponents.sessionManager.selectedSession) {
                requireComponents.sessionManager.remove(session)
            }
        }

        // Purposefully not calling onAuthSuccess in case we add to that function in the future
        view!!.alpha = 1f
    }

    override fun biometricCreateNewSession() {
        requireComponents.sessionManager.removeSessions()
    }

    override fun onAuthSuccess() {
        view!!.alpha = 1f
    }

    override fun onResume() {
        super.onResume()

        StatusBarUtils.getStatusBarHeight(statusBar) { statusBarHeight ->
            statusBar!!.layoutParams.height = statusBarHeight
        }

        if (Biometrics.isBiometricsEnabled(requireContext())) {
            if (biometricController == null) {
                biometricController = BiometricAuthenticationHandler(context!!)
            }

            displayBiometricPromptIfNeeded()
        } else {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                biometricController?.stopListening()
            }

            biometricController = null
            view!!.alpha = 1f
        }
    }

    @RequiresApi(api = Build.VERSION_CODES.M)
    private fun displayBiometricPromptIfNeeded() {
        val fragmentManager = childFragmentManager

        // Check that we need to auth and that the fragment isn't already displayed
        if (biometricController!!.needsAuth || openedFromExternalLink) {
            view!!.alpha = 0f
            biometricController!!.startAuthentication(openedFromExternalLink)
            openedFromExternalLink = false

            // Are we already displaying the biometric fragment?
            if (fragmentManager.findFragmentByTag(BiometricAuthenticationDialogFragment.FRAGMENT_TAG) != null) {
                return
            }

            try {
                biometricController!!.biometricFragment!!.show(
                    fragmentManager,
                    BiometricAuthenticationDialogFragment.FRAGMENT_TAG
                )
            } catch (e: IllegalStateException) {
                // It can happen that at this point in time the activity is already in the background
                // and onSaveInstanceState() has already been called. Fragment transactions are not
                // allowed after that anymore.
            }
        } else {
            view!!.alpha = 1f
        }
    }

    @Suppress("ComplexMethod", "ReturnCount")
    fun onBackPressed(): Boolean {
        if (findInPageIntegration.onBackPressed()) {
            return true
        } else if (fullScreenIntegration.onBackPressed()) {
            return true
        } else if (sessionFeature.get()?.onBackPressed() == true) {
            return true
        } else {
            if (session.source == SessionState.Source.ACTION_VIEW || session.isCustomTabSession()) {
                TelemetryWrapper.eraseBackToAppEvent()

                // This session has been started from a VIEW intent. Go back to the previous app
                // immediately and erase the current browsing session.
                erase()

                // If there are no other sessions then we remove the whole task because otherwise
                // the old session might still be partially visible in the app switcher.
                if (requireComponents.sessionManager.sessions.isEmpty()) {
                    requireActivity().finishAndRemoveTask()
                } else {
                    requireActivity().finish()
                }
                // We can't show a snackbar outside of the app. So let's show a toast instead.
                Toast.makeText(context, R.string.feedback_erase_custom_tab, Toast.LENGTH_SHORT).show()
            } else {
                // Just go back to the home screen.
                TelemetryWrapper.eraseBackToHomeEvent()

                erase()
            }
        }

        return true
    }

    fun erase() {
        val context = context

        // Notify the user their session has been erased if Talk Back is enabled:
        if (context != null) {
            val manager = context.getSystemService(Context.ACCESSIBILITY_SERVICE) as AccessibilityManager
            if (manager.isEnabled) {
                val event = AccessibilityEvent.obtain()
                event.eventType = AccessibilityEvent.TYPE_ANNOUNCEMENT
                event.className = javaClass.name
                event.packageName = getContext()!!.packageName
                event.text.add(getString(R.string.feedback_erase))
            }
        }

        requireComponents.sessionManager.remove(session)

        // Temporary workaround until we get https://bugzilla.mozilla.org/show_bug.cgi?id=1644156
        // See comment in TabUtils.createTab().
        requireComponents.engine.clearData(Engine.BrowsingData.all())
    }

    private fun shareCurrentUrl() {
        val shareIntent = Intent(Intent.ACTION_SEND)
        shareIntent.type = "text/plain"
        shareIntent.putExtra(Intent.EXTRA_TEXT, session.url)

        val title = session.title
        if (title.isNotEmpty()) {
            shareIntent.putExtra(Intent.EXTRA_SUBJECT, title)
        }

        startActivity(Intent.createChooser(shareIntent, getString(R.string.share_dialog_title)))

        TelemetryWrapper.shareEvent()
    }

    @Suppress("ComplexMethod")
    override fun onClick(view: View) {
        when (view.id) {
            R.id.display_url -> if (
                    !crashReporterIsVisible() &&
                    requireComponents.sessionManager.findSessionById(session.id) != null) {
                val urlFragment = UrlInputFragment
                    .createWithSession(session, urlView!!)

                requireActivity().supportFragmentManager
                    .beginTransaction()
                    .add(R.id.container, urlFragment, UrlInputFragment.FRAGMENT_TAG)
                    .commit()
            }

            R.id.erase -> {
                TelemetryWrapper.eraseEvent()

                erase()
            }

            R.id.tabs -> {
                requireActivity().supportFragmentManager
                    .beginTransaction()
                    .add(R.id.container, SessionsSheetFragment(), SessionsSheetFragment.FRAGMENT_TAG)
                    .commit()

                TelemetryWrapper.openTabsTrayEvent()
            }

            R.id.back -> {
                requireComponents.sessionUseCases.goBack(session)
            }

            R.id.forward -> {
                requireComponents.sessionUseCases.goForward(session)
            }

            R.id.refresh -> {
                requireComponents.sessionUseCases.reload(session)

                TelemetryWrapper.menuReloadEvent()
            }

            R.id.stop -> {
                requireComponents.sessionUseCases.stopLoading(session)
            }

            R.id.open_in_firefox_focus -> {
                // Release the session from this view so that it can immediately be rendered by a different view
                engineView!!.release()

                // Strip the CustomTabConfig to turn this Session into a regular tab and then select it
                session.customTabConfig = null
                requireComponents.sessionManager.select(session)

                val intent = Intent(context, MainActivity::class.java)
                intent.action = Intent.ACTION_MAIN
                intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP
                startActivity(intent)

                TelemetryWrapper.openFullBrowser()

                val activity = activity
                activity?.finish()
            }

            R.id.share -> {
                shareCurrentUrl()
            }

            R.id.settings -> (activity as LocaleAwareAppCompatActivity).openPreferences()

            R.id.open_default -> {
                val browsers = Browsers(requireContext(), session.url)

                val defaultBrowser = browsers.defaultBrowser
                    ?: throw IllegalStateException("<Open with \$Default> was shown when no default browser is set")
                    // We only add this menu item when a third party default exists, in
                    // BrowserMenuAdapter.initializeMenu()

                val intent = Intent(Intent.ACTION_VIEW, Uri.parse(session.url))
                intent.setPackage(defaultBrowser.packageName)
                startActivity(intent)

                if (browsers.isFirefoxDefaultBrowser) {
                    TelemetryWrapper.openFirefoxEvent()
                } else {
                    TelemetryWrapper.openDefaultAppEvent()
                }
            }

            R.id.open_select_browser -> {
                val browsers = Browsers(requireContext(), session.url)

                val apps = browsers.installedBrowsers
                val store = if (browsers.hasFirefoxBrandedBrowserInstalled())
                    null
                else
                    InstallFirefoxActivity.resolveAppStore(requireContext())

                val fragment = OpenWithFragment.newInstance(
                    apps,
                    session.url,
                    store
                )
                @Suppress("DEPRECATION")
                fragment.show(requireFragmentManager(), OpenWithFragment.FRAGMENT_TAG)

                TelemetryWrapper.openSelectionEvent()
            }

            R.id.customtab_close -> {
                erase()
                requireActivity().finish()

                TelemetryWrapper.closeCustomTabEvent()
            }

            R.id.help -> {
                val session = createTab(SupportUtils.HELP_URL, source = SessionState.Source.MENU)
                requireComponents.sessionManager.add(session, selected = true)
            }

            R.id.help_trackers -> {
                val url = SupportUtils.getSumoURLForTopic(context!!, SupportUtils.SumoTopic.TRACKERS)
                val session = createTab(url, source = SessionState.Source.MENU)

                requireComponents.sessionManager.add(session, selected = true)
            }

            R.id.add_to_homescreen -> {
                showAddToHomescreenDialog(
                    session.url, session.title
                )
            }

            R.id.report_site_issue -> {
                val reportUrl = String.format(SupportUtils.REPORT_SITE_ISSUE_URL, session.url)
                val session = createTab(reportUrl, source = SessionState.Source.MENU)
                requireComponents.sessionManager.add(session, selected = true)

                TelemetryWrapper.reportSiteIssueEvent()
            }

            R.id.find_in_page -> {
                val sessionState = requireComponents.store.state.findTab(session.id)
                if (sessionState != null) {
                    findInPageIntegration.get()?.show(sessionState)
                }
                TelemetryWrapper.findInPageMenuEvent()
            }

            else -> throw IllegalArgumentException("Unhandled menu item in BrowserFragment")
        }
    }

    fun setShouldRequestDesktop(enabled: Boolean) {
        if (enabled) {
            PreferenceManager.getDefaultSharedPreferences(context).edit()
                .putBoolean(
                    requireContext().getString(R.string.has_requested_desktop),
                    true
                ).apply()
        }

        requireComponents.sessionUseCases.requestDesktopSite(enabled, session)
    }

    fun showSecurityPopUp() {
        if (crashReporterIsVisible()) {
            return
        }

        // Don't show Security Popup if the page is loading
        if (session.loading) {
            return
        }
        val securityPopup = PopupUtils.createSecurityPopup(requireContext(), session)
        if (securityPopup != null) {
            securityPopup.setOnDismissListener { popupTint!!.visibility = View.GONE }
            securityPopup.setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
            securityPopup.animationStyle = android.R.style.Animation_Dialog
            securityPopup.isTouchable = true
            securityPopup.isFocusable = true
            securityPopup.elevation = resources.getDimension(R.dimen.menu_elevation)
            val offsetY = requireContext().resources.getDimensionPixelOffset(R.dimen.doorhanger_offsetY)
            securityPopup.showAtLocation(urlBar, Gravity.TOP or Gravity.CENTER_HORIZONTAL, 0, offsetY)
            popupTint!!.visibility = View.VISIBLE
        }
    }

    override fun onLongClick(view: View): Boolean {
        // Detect long clicks on display_url
        if (view.id == R.id.display_url) {
            val context = activity ?: return false

            if (session.isCustomTabSession()) {
                val clipBoard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                val uri = Uri.parse(session.url)
                clipBoard.setPrimaryClip(ClipData.newRawUri("Uri", uri))
                Toast.makeText(context, getString(R.string.custom_tab_copy_url_action), Toast.LENGTH_SHORT).show()
            }
        }

        return false
    }

    override fun applyLocale() {
        activity?.supportFragmentManager
            ?.beginTransaction()
            ?.replace(
                R.id.container,
                BrowserFragment.createForSession(requireNotNull(session)),
                BrowserFragment.FRAGMENT_TAG
            )
            ?.commit()
    }

    fun handleTabCrash(crash: Crash) {
        showCrashReporter(crash)
    }

    companion object {
        const val FRAGMENT_TAG = "browser"

        private const val ARGUMENT_SESSION_UUID = "sessionUUID"

        @JvmStatic
        fun createForSession(session: Session): BrowserFragment {
            val arguments = Bundle()
            arguments.putString(ARGUMENT_SESSION_UUID, session.id)

            val fragment = BrowserFragment()
            fragment.arguments = arguments

            return fragment
        }
    }
}
