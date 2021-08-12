/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import kotlinx.android.extensions.LayoutContainer
import org.mozilla.fenix.databinding.ShareToAppsBinding
import org.mozilla.fenix.share.listadapters.AppShareAdapter
import org.mozilla.fenix.share.listadapters.AppShareOption

/**
 * Callbacks for possible user interactions on the [ShareCloseView]
 */
interface ShareToAppsInteractor {
    fun onShareToApp(appToShareTo: AppShareOption)
}

class ShareToAppsView(
    override val containerView: ViewGroup,
    interactor: ShareToAppsInteractor
) : LayoutContainer {

    private val adapter = AppShareAdapter(interactor)
    private val recentAdapter = AppShareAdapter(interactor)
    private var binding: ShareToAppsBinding = ShareToAppsBinding.inflate(
        LayoutInflater.from(containerView.context),
        containerView,
        true
    )

    init {
        binding.appsList.adapter = adapter
        binding.recentAppsList.adapter = recentAdapter
    }

    fun setShareTargets(targets: List<AppShareOption>) {
        binding.progressBar.visibility = View.GONE

        binding.appsList.visibility = View.VISIBLE
        adapter.submitList(targets)
    }

    fun setRecentShareTargets(recentTargets: List<AppShareOption>) {
        if (recentTargets.isEmpty()) {
            binding.recentAppsContainer.visibility = View.GONE
            return
        }
        binding.progressBar.visibility = View.GONE

        binding.recentAppsContainer.visibility = View.VISIBLE
        recentAdapter.submitList(recentTargets)
    }
}
