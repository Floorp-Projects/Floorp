/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import kotlinx.android.extensions.LayoutContainer
import org.mozilla.fenix.R
import org.mozilla.fenix.share.listadapters.AppShareOption
import kotlinx.android.synthetic.main.share_to_apps.*
import org.mozilla.fenix.share.listadapters.AppShareAdapter

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
    init {
        LayoutInflater.from(containerView.context)
            .inflate(R.layout.share_to_apps, containerView, true)

        appsList.adapter = AppShareAdapter(interactor)
    }

    fun setSharetargets(targets: List<AppShareOption>) {
        progressBar.visibility = View.GONE
        appsList.visibility = View.VISIBLE

        with(appsList.adapter as AppShareAdapter) {
            updateData(targets)
        }
    }
}
