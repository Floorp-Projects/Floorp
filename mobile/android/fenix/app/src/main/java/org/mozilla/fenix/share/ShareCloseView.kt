/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.LinearLayoutManager
import mozilla.components.concept.engine.prompt.ShareData
import org.mozilla.fenix.databinding.ShareCloseBinding
import org.mozilla.fenix.share.listadapters.ShareTabsAdapter

/**
 * Callbacks for possible user interactions on the [ShareCloseView]
 */
interface ShareCloseInteractor {
    fun onShareClosed()
}

class ShareCloseView(
    val containerView: ViewGroup,
    private val interactor: ShareCloseInteractor,
) {

    val adapter = ShareTabsAdapter()

    init {
        val binding = ShareCloseBinding.inflate(
            LayoutInflater.from(containerView.context),
            containerView,
            true
        )

        binding.closeButton.setOnClickListener { interactor.onShareClosed() }

        binding.sharedSiteList.layoutManager = LinearLayoutManager(containerView.context)
        binding.sharedSiteList.adapter = adapter
    }

    fun setTabs(tabs: List<ShareData>) {
        adapter.submitList(tabs)
    }
}
