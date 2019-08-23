/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import android.view.LayoutInflater
import android.view.ViewGroup
import kotlinx.android.extensions.LayoutContainer
import org.mozilla.fenix.R
import org.mozilla.fenix.share.listadapters.Application

/**
 * Callbacks for possible user interactions on the [ShareCloseView]
 */
interface ShareToAppsInteractor {
    fun onShareToApp(appToShareTo: Application)
}

class ShareToAppsView(
    override val containerView: ViewGroup,
    private val interactor: ShareToAppsInteractor
) : LayoutContainer {
    init {
        LayoutInflater.from(containerView.context)
            .inflate(R.layout.share_to_apps, containerView, true)
    }
}
