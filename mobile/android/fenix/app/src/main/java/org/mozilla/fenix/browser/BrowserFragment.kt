/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.annotation.SuppressLint
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.transition.TransitionInflater
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.accessibility.AccessibilityManager
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.fragment.app.Fragment
import kotlinx.android.synthetic.main.fragment_browser.*
import mozilla.components.feature.downloads.DownloadsFeature
import mozilla.components.feature.session.SessionFeature
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.ktx.android.arch.lifecycle.addObservers
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.ToolbarIntegration
import org.mozilla.fenix.ext.requireComponents
import mozilla.components.feature.prompts.PromptFeature

class BrowserFragment : Fragment() {

    private lateinit var downloadsFeature: DownloadsFeature
    private lateinit var promptsFeature: PromptFeature
    private lateinit var sessionFeature: SessionFeature

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        return inflater.inflate(R.layout.fragment_browser, container, false)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        sharedElementEnterTransition = TransitionInflater.from(context).inflateTransition(android.R.transition.move)
    }

    @SuppressLint("CheckResult")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val sessionManager = requireComponents.core.sessionManager

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
            engineView
        )

        // Stop toolbar from collapsing if TalkBack is enabled
        val accessibilityManager = context?.getSystemService(Context.ACCESSIBILITY_SERVICE) as AccessibilityManager
        if (accessibilityManager.isEnabled) {
            val layoutParams = toolbar.layoutParams as CoordinatorLayout.LayoutParams
            layoutParams.behavior = null
        }

        lifecycle.addObservers(
            downloadsFeature,
            promptsFeature,
            sessionFeature,
            ToolbarIntegration(requireContext(),
                toolbar,
                requireComponents.toolbar.shippedDomainsProvider,
                requireComponents.core.historyStorage)
        )
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

    companion object {
        private const val REQUEST_CODE_DOWNLOAD_PERMISSIONS = 1
        private const val REQUEST_CODE_PROMPT_PERMISSIONS = 2
    }
}
