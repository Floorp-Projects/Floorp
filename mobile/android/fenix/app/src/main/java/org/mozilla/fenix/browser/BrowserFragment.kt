/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.accessibility.AccessibilityManager
import androidx.appcompat.app.AppCompatActivity
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import androidx.navigation.Navigation
import com.google.android.material.snackbar.Snackbar
import kotlinx.android.synthetic.main.component_search.*
import kotlinx.android.synthetic.main.fragment_browser.view.*
import mozilla.components.browser.toolbar.behavior.BrowserToolbarBottomBehavior
import mozilla.components.feature.contextmenu.ContextMenuCandidate
import mozilla.components.feature.contextmenu.ContextMenuFeature
import mozilla.components.feature.customtabs.CustomTabsToolbarFeature
import mozilla.components.feature.downloads.DownloadsFeature
import mozilla.components.feature.prompts.PromptFeature
import mozilla.components.feature.session.FullScreenFeature
import mozilla.components.feature.session.SessionFeature
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.sitepermissions.SitePermissionsFeature
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.ktx.android.view.enterToImmersiveMode
import mozilla.components.support.ktx.android.view.exitImmersiveModeIfNeeded
import org.mozilla.fenix.BrowsingModeManager
import org.mozilla.fenix.DefaultThemeManager
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FindInPageIntegration
import org.mozilla.fenix.components.toolbar.SearchAction
import org.mozilla.fenix.components.toolbar.SearchState
import org.mozilla.fenix.components.toolbar.ToolbarComponent
import org.mozilla.fenix.components.toolbar.ToolbarIntegration
import org.mozilla.fenix.components.toolbar.ToolbarMenu
import org.mozilla.fenix.components.toolbar.ToolbarUIView
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.share
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.getAutoDisposeObservable

class BrowserFragment : Fragment(), BackHandler {
    private lateinit var toolbarComponent: ToolbarComponent

    private val sessionFeature = ViewBoundFeatureWrapper<SessionFeature>()
    private val contextMenuFeature = ViewBoundFeatureWrapper<ContextMenuFeature>()
    private val downloadsFeature = ViewBoundFeatureWrapper<DownloadsFeature>()
    private val promptsFeature = ViewBoundFeatureWrapper<PromptFeature>()
    private val findInPageIntegration = ViewBoundFeatureWrapper<FindInPageIntegration>()
    private val customTabsToolbarFeature = ViewBoundFeatureWrapper<CustomTabsToolbarFeature>()
    private val toolbarIntegration = ViewBoundFeatureWrapper<ToolbarIntegration>()
    private val sitePermissionsFeature = ViewBoundFeatureWrapper<SitePermissionsFeature>()
    private val fullScreenFeature = ViewBoundFeatureWrapper<FullScreenFeature>()
    var sessionId: String? = null

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        require(arguments != null)
        sessionId = BrowserFragmentArgs.fromBundle(arguments!!).sessionId

        val view = inflater.inflate(R.layout.fragment_browser, container, false)

        toolbarComponent = ToolbarComponent(
            view.browserLayout,
            ActionBusFactory.get(this), sessionId,
            (activity as HomeActivity).browsingModeManager.isPrivate,
            SearchState("", isEditing = false)
        )

        toolbarComponent.uiView.view.apply {
            setBackgroundColor(ContextCompat.getColor(view.context,
                DefaultThemeManager.resolveAttribute(R.attr.browserToolbarBackground, context)))

            (layoutParams as CoordinatorLayout.LayoutParams).apply {
                // Stop toolbar from collapsing if TalkBack is enabled
                val accessibilityManager = context
                    ?.getSystemService(Context.ACCESSIBILITY_SERVICE) as AccessibilityManager

                if (!accessibilityManager.isTouchExplorationEnabled) {
                    behavior = BrowserToolbarBottomBehavior(view.context, null)
                }

                gravity = Gravity.BOTTOM
                height = (resources.displayMetrics.density * TOOLBAR_HEIGHT).toInt()
            }
        }

        val activity = activity as HomeActivity
        DefaultThemeManager.applyStatusBarTheme(activity.window, activity.themeManager, activity, false)

        return view
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        sessionId = BrowserFragmentArgs.fromBundle(arguments!!).sessionId

        val sessionManager = requireComponents.core.sessionManager

        contextMenuFeature.set(
            feature = ContextMenuFeature(
                requireFragmentManager(),
                sessionManager,
                ContextMenuCandidate.defaultCandidates(
                    requireContext(),
                    requireComponents.useCases.tabsUseCases,
                    view),
                view.engineView),
            owner = this,
            view = view)

        downloadsFeature.set(
            feature = DownloadsFeature(
                requireContext(),
                sessionManager = sessionManager,
                fragmentManager = childFragmentManager,
                onNeedToRequestPermissions = { permissions ->
                    requestPermissions(permissions, REQUEST_CODE_DOWNLOAD_PERMISSIONS)
                }),
            owner = this,
            view = view)

        promptsFeature.set(
            feature = PromptFeature(
                fragment = this,
                sessionManager = sessionManager,
                fragmentManager = requireFragmentManager(),
                onNeedToRequestPermissions = { permissions ->
                    requestPermissions(permissions, REQUEST_CODE_PROMPT_PERMISSIONS)
                }),
            owner = this,
            view = view)

        sessionFeature.set(
            feature = SessionFeature(
                sessionManager,
                SessionUseCases(sessionManager),
                view.engineView,
                sessionId),
            owner = this,
            view = view)

        findInPageIntegration.set(
            feature = FindInPageIntegration(requireComponents.core.sessionManager, view.findInPageView),
            owner = this,
            view = view)

        customTabsToolbarFeature.set(
            feature = CustomTabsToolbarFeature(
                sessionManager,
                toolbarComponent.getView(),
                sessionId,
                closeListener = { requireActivity().finish() }),
            owner = this,
            view = view)

        toolbarIntegration.set(
            feature = (toolbarComponent.uiView as ToolbarUIView).toolbarIntegration,
            owner = this,
            view = view)

        sitePermissionsFeature.set(
            feature = SitePermissionsFeature(
                anchorView = view.findInPageView,
                sessionManager = sessionManager
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
                sessionId
            ) {
                if (it) {
                    Snackbar.make(view.rootView, getString(R.string.full_screen_notification), Snackbar.LENGTH_LONG)
                        .show()
                    activity?.enterToImmersiveMode()
                    toolbar.visibility = View.GONE
                } else {
                    activity?.exitImmersiveModeIfNeeded()
                    toolbar.visibility = View.VISIBLE
                }
            },
            owner = this,
            view = view
        )
    }

    override fun onResume() {
        super.onResume()
        (activity as AppCompatActivity).supportActionBar?.hide()
    }

    override fun onStart() {
        super.onStart()
        getAutoDisposeObservable<SearchAction>()
            .subscribe {
                when (it) {
                    is SearchAction.ToolbarTapped -> Navigation.findNavController(toolbarComponent.getView())
                        .navigate(
                            BrowserFragmentDirections.actionBrowserFragmentToSearchFragment(
                                requireComponents.core.sessionManager.selectedSession?.id)
                        )
                    is SearchAction.ToolbarMenuItemTapped -> handleToolbarItemInteraction(it)
                }
            }
    }

    override fun onBackPressed(): Boolean {
        return when {
            findInPageIntegration.onBackPressed() -> true
            sessionFeature.onBackPressed() -> true
            customTabsToolbarFeature.onBackPressed() -> true
            else -> false
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
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

    // This method triggers the complexity warning. However it's actually not that hard to understand.
    @SuppressWarnings("ComplexMethod")
    private fun handleToolbarItemInteraction(action: SearchAction.ToolbarMenuItemTapped) {
        val sessionUseCases = requireComponents.useCases.sessionUseCases
        Do exhaustive when (action.item) {
            ToolbarMenu.Item.Back -> sessionUseCases.goBack.invoke()
            ToolbarMenu.Item.Forward -> sessionUseCases.goForward.invoke()
            ToolbarMenu.Item.Reload -> sessionUseCases.reload.invoke()
            ToolbarMenu.Item.Settings -> Navigation.findNavController(toolbarComponent.getView())
                .navigate(BrowserFragmentDirections.actionBrowserFragmentToSettingsFragment())
            ToolbarMenu.Item.Library -> Navigation.findNavController(toolbarComponent.getView())
                .navigate(BrowserFragmentDirections.actionBrowserFragmentToLibraryFragment())
            is ToolbarMenu.Item.RequestDesktop -> sessionUseCases.requestDesktopSite.invoke(action.item.isChecked)
            ToolbarMenu.Item.Share -> requireComponents.core.sessionManager
                .selectedSession?.url?.apply { requireContext().share(this) }
            ToolbarMenu.Item.NewPrivateTab -> {
                val directions = BrowserFragmentDirections
                    .actionBrowserFragmentToSearchFragment(null)
                Navigation.findNavController(view!!).navigate(directions)
                (activity as HomeActivity).browsingModeManager.mode = BrowsingModeManager.Mode.Private
            }
            ToolbarMenu.Item.FindInPage -> FindInPageIntegration.launch?.invoke()
            ToolbarMenu.Item.ReportIssue -> requireComponents.core.sessionManager
                .selectedSession?.url?.apply {
                val reportUrl = String.format(REPORT_SITE_ISSUE_URL, this)
                sessionUseCases.loadUrl.invoke(reportUrl)
            }
            ToolbarMenu.Item.Help -> {
                // TODO Help
            }
            ToolbarMenu.Item.NewTab -> {
                val directions = BrowserFragmentDirections
                    .actionBrowserFragmentToSearchFragment(null)
                Navigation.findNavController(view!!).navigate(directions)
                (activity as HomeActivity).browsingModeManager.mode = BrowsingModeManager.Mode.Normal
            }
        }
    }

    object Do {
        inline infix fun <reified T> exhaustive(any: T?) = any
    }

    companion object {
        private const val REQUEST_CODE_DOWNLOAD_PERMISSIONS = 1
        private const val REQUEST_CODE_PROMPT_PERMISSIONS = 2
        private const val REQUEST_CODE_APP_PERMISSIONS = 3
        private const val TOOLBAR_HEIGHT = 56f
        private const val REPORT_SITE_ISSUE_URL = "https://webcompat.com/issues/new?url=%s&label=browser-fenix"
    }
}
