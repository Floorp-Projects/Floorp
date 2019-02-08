/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.annotation.SuppressLint
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.accessibility.AccessibilityManager
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
import mozilla.components.feature.session.SessionFeature
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.ktx.android.arch.lifecycle.addObservers
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.requireComponents
import mozilla.components.feature.prompts.PromptFeature
import org.mozilla.fenix.BackHandler
import org.mozilla.fenix.components.FindInPageIntegration
import org.mozilla.fenix.ext.share
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.getSafeManagedObservable
import org.mozilla.fenix.search.toolbar.SearchAction
import org.mozilla.fenix.search.toolbar.ToolbarComponent
import org.mozilla.fenix.search.toolbar.SearchState
import org.mozilla.fenix.search.toolbar.ToolbarUIView
import org.mozilla.fenix.search.toolbar.ToolbarMenu

class BrowserFragment : Fragment(), BackHandler {

    private lateinit var contextMenuFeature: ContextMenuFeature
    private lateinit var downloadsFeature: DownloadsFeature
    private lateinit var findInPageIntegration: FindInPageIntegration
    private lateinit var promptsFeature: PromptFeature
    private lateinit var sessionFeature: SessionFeature
    private lateinit var toolbarComponent: ToolbarComponent
    private lateinit var customTabsToolbarFeature: CustomTabsToolbarFeature

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_browser, container, false)
        toolbarComponent = ToolbarComponent(
            view.browserLayout,
            ActionBusFactory.get(this),
            SearchState("", isEditing = false)
        )

        toolbarComponent.uiView.view.apply {
            setBackgroundColor(ContextCompat.getColor(view.context, R.color.offwhite))

            (layoutParams as CoordinatorLayout.LayoutParams).apply {
                // Stop toolbar from collapsing if TalkBack is enabled
                val accessibilityManager = context
                    ?.getSystemService(Context.ACCESSIBILITY_SERVICE) as AccessibilityManager

                if (!accessibilityManager.isEnabled) {
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
                        .navigate(R.id.action_browserFragment_to_searchFragment, null, null)
                    is SearchAction.ToolbarMenuItemTapped -> handleToolbarItemInteraction(it)
                }
            }
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val sessionId = arguments?.getString(SESSION_ID)

        val sessionManager = requireComponents.core.sessionManager

        contextMenuFeature = ContextMenuFeature(
            requireFragmentManager(),
            sessionManager,
            ContextMenuCandidate.defaultCandidates(
                requireContext(),
                requireComponents.useCases.tabsUseCases,
                view),
            view.engineView)

        downloadsFeature = DownloadsFeature(
            requireContext(),
            sessionManager = sessionManager,
            fragmentManager = childFragmentManager,
            onNeedToRequestPermissions = { permissions ->
                requestPermissions(permissions, REQUEST_CODE_DOWNLOAD_PERMISSIONS)
            }
        )

        promptsFeature = PromptFeature(
            fragment = this,
            sessionManager = sessionManager,
            fragmentManager = requireFragmentManager(),
            onNeedToRequestPermissions = { permissions ->
                requestPermissions(permissions, REQUEST_CODE_PROMPT_PERMISSIONS)
            }
        )

        sessionFeature = SessionFeature(
            sessionManager,
            SessionUseCases(sessionManager),
            view.engineView,
            sessionId
        )

        findInPageIntegration = FindInPageIntegration(requireComponents.core.sessionManager, view.findInPageView)

        customTabsToolbarFeature = CustomTabsToolbarFeature(
            sessionManager,
            toolbar,
            sessionId
        ) { requireActivity().finish() }

        lifecycle.addObservers(
            contextMenuFeature,
            downloadsFeature,
            findInPageIntegration,
            promptsFeature,
            sessionFeature,
            (toolbarComponent.uiView as ToolbarUIView).toolbarIntegration,
            customTabsToolbarFeature
        )
    }

    override fun onDestroyView() {
        super.onDestroyView()

        lifecycle.removeObserver(sessionFeature)
    }

    @SuppressWarnings("ReturnCount")
    override fun onBackPressed(): Boolean {
        if (findInPageIntegration.onBackPressed()) return true
        if (sessionFeature.onBackPressed()) return true
        if (customTabsToolbarFeature.onBackPressed()) return true

        // We'll want to improve this when we add multitasking
        requireComponents.core.sessionManager.remove()
        return false
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
        when (requestCode) {
            REQUEST_CODE_DOWNLOAD_PERMISSIONS -> downloadsFeature.onPermissionsResult(permissions, grantResults)
            REQUEST_CODE_PROMPT_PERMISSIONS -> promptsFeature.onPermissionsResult(permissions, grantResults)
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        promptsFeature.onActivityResult(requestCode, resultCode, data)
    }

    // This method triggers the complexity warning. However it's actually not that hard to understand.
    @SuppressWarnings("ComplexMethod")
    private fun handleToolbarItemInteraction(action: SearchAction.ToolbarMenuItemTapped) {
        val sessionUseCases = requireComponents.useCases.sessionUseCases
        when (action.item) {
            is ToolbarMenu.Item.Back -> sessionUseCases.goBack.invoke()
            is ToolbarMenu.Item.Forward -> sessionUseCases.goForward.invoke()
            is ToolbarMenu.Item.Reload -> sessionUseCases.reload.invoke()
            is ToolbarMenu.Item.Settings -> Navigation.findNavController(toolbar)
                .navigate(R.id.action_browserFragment_to_settingsActivity, null, null)
            is ToolbarMenu.Item.Library -> Navigation.findNavController(toolbar)
                .navigate(R.id.action_browserFragment_to_libraryFragment, null, null)
            is ToolbarMenu.Item.RequestDesktop -> sessionUseCases.requestDesktopSite.invoke(action.item.isChecked)
            is ToolbarMenu.Item.Share -> requireComponents.core.sessionManager
                .selectedSession?.url?.apply { requireContext().share(this) }
        }
    }

    companion object {
        const val SESSION_ID = "session_id"
        private const val REQUEST_CODE_DOWNLOAD_PERMISSIONS = 1
        private const val REQUEST_CODE_PROMPT_PERMISSIONS = 2
        private const val TOOLBAR_HEIGHT = 56f
    }
}
