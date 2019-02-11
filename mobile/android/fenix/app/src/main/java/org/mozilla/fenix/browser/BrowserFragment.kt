/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.annotation.SuppressLint
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.preference.PreferenceManager
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
import kotlinx.android.synthetic.main.component_search.*
import kotlinx.android.synthetic.main.fragment_browser.view.*
import mozilla.components.browser.toolbar.behavior.BrowserToolbarBottomBehavior
import mozilla.components.feature.contextmenu.ContextMenuCandidate
import mozilla.components.feature.contextmenu.ContextMenuFeature
import mozilla.components.feature.customtabs.CustomTabsToolbarFeature
import mozilla.components.feature.downloads.DownloadsFeature
import mozilla.components.feature.prompts.PromptFeature
import mozilla.components.feature.session.SessionFeature
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.mozilla.fenix.DefaultThemeManager
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FindInPageIntegration
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.share
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.getSafeManagedObservable
import org.mozilla.fenix.search.toolbar.SearchAction
import org.mozilla.fenix.search.toolbar.SearchState
import org.mozilla.fenix.search.toolbar.ToolbarComponent
import org.mozilla.fenix.search.toolbar.ToolbarIntegration
import org.mozilla.fenix.search.toolbar.ToolbarMenu
import org.mozilla.fenix.search.toolbar.ToolbarUIView

class BrowserFragment : Fragment(), BackHandler {
    private lateinit var toolbarComponent: ToolbarComponent

    private val sessionFeature = ViewBoundFeatureWrapper<SessionFeature>()
    private val contextMenuFeature = ViewBoundFeatureWrapper<ContextMenuFeature>()
    private val downloadsFeature = ViewBoundFeatureWrapper<DownloadsFeature>()
    private val promptsFeature = ViewBoundFeatureWrapper<PromptFeature>()
    private val findInPageIntegration = ViewBoundFeatureWrapper<FindInPageIntegration>()
    private val customTabsToolbarFeature = ViewBoundFeatureWrapper<CustomTabsToolbarFeature>()
    private val toolbarIntegration = ViewBoundFeatureWrapper<ToolbarIntegration>()
    var sessionId: String? = null

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        sessionId = BrowserFragmentArgs.fromBundle(arguments!!).sessionId
        val view = inflater.inflate(R.layout.fragment_browser, container, false)

        toolbarComponent = ToolbarComponent(
            view.browserLayout,
            ActionBusFactory.get(this), sessionId,
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

        return view
    }

    @SuppressLint("CheckResult")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        getSafeManagedObservable<SearchAction>()
            .subscribe {
                when (it) {
                    is SearchAction.ToolbarTapped -> Navigation.findNavController(toolbar)
                        .navigate(BrowserFragmentDirections.actionBrowserFragmentToSearchFragment(
                            requireComponents.core.sessionManager.selectedSession?.id
                        ))
                    is SearchAction.ToolbarMenuItemTapped -> handleToolbarItemInteraction(it)
                }
            }
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        sessionId = BrowserFragmentArgs.fromBundle(arguments!!).sessionId

        (activity as AppCompatActivity).supportActionBar?.hide()

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
                toolbar,
                sessionId,
                closeListener = { requireActivity().finish() }),
            owner = this,
            view = view)

        toolbarIntegration.set(
            feature = (toolbarComponent.uiView as ToolbarUIView).toolbarIntegration,
            owner = this,
            view = view)
    }

    @SuppressWarnings("ReturnCount")
    override fun onBackPressed(): Boolean {
        if (findInPageIntegration.onBackPressed()) return true
        if (sessionFeature.onBackPressed()) return true
        if (customTabsToolbarFeature.onBackPressed()) return true

        return false
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
        when (requestCode) {
            REQUEST_CODE_DOWNLOAD_PERMISSIONS -> downloadsFeature.withFeature {
                it.onPermissionsResult(permissions, grantResults)
            }
            REQUEST_CODE_PROMPT_PERMISSIONS -> promptsFeature.withFeature {
                it.onPermissionsResult(permissions, grantResults)
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
            is ToolbarMenu.Item.Back -> sessionUseCases.goBack.invoke()
            is ToolbarMenu.Item.Forward -> sessionUseCases.goForward.invoke()
            is ToolbarMenu.Item.Reload -> sessionUseCases.reload.invoke()
            is ToolbarMenu.Item.Settings -> Navigation.findNavController(toolbar)
                .navigate(BrowserFragmentDirections.actionBrowserFragmentToSettingsFragment())
            is ToolbarMenu.Item.Library -> Navigation.findNavController(toolbar)
                .navigate(BrowserFragmentDirections.actionBrowserFragmentToLibraryFragment())
            is ToolbarMenu.Item.RequestDesktop -> sessionUseCases.requestDesktopSite.invoke(action.item.isChecked)
            is ToolbarMenu.Item.Share -> requireComponents.core.sessionManager
                .selectedSession?.url?.apply { requireContext().share(this) }
            is ToolbarMenu.Item.NewPrivateTab -> {
                PreferenceManager.getDefaultSharedPreferences(context)
                    .edit().putBoolean(
                        context!!.getString(R.string.pref_key_private_mode),
                        !PreferenceManager.getDefaultSharedPreferences(context)
                            .getBoolean(context!!.getString(R.string.pref_key_private_mode), false)
                    ).apply()
            }
            is ToolbarMenu.Item.FindInPage -> FindInPageIntegration.launch?.invoke()
            is ToolbarMenu.Item.ReportIssue -> requireComponents.core.sessionManager
                .selectedSession?.url?.apply {
                val reportUrl = String.format(REPORT_SITE_ISSUE_URL, this)
                sessionUseCases.loadUrl.invoke(reportUrl)
            }
            ToolbarMenu.Item.Help -> {
                // TODO Help
            }
            ToolbarMenu.Item.NewTab -> {
                val directions =
                    BrowserFragmentDirections.actionBrowserFragmentToSearchFragment(null)
                Navigation.findNavController(view!!).navigate(directions)
            }
        }
    }

    object Do {
        inline infix fun <reified T> exhaustive(any: T?) = any
    }

    companion object {
        private const val REQUEST_CODE_DOWNLOAD_PERMISSIONS = 1
        private const val REQUEST_CODE_PROMPT_PERMISSIONS = 2
        private const val TOOLBAR_HEIGHT = 56f
        private const val REPORT_SITE_ISSUE_URL = "https://webcompat.com/issues/new?url=%s&label=browser-fenix"
    }
}
