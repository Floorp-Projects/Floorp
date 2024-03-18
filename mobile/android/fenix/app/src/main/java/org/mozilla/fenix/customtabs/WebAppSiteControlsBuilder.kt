/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customtabs

import android.app.Notification
import android.content.Context
import android.content.Intent
import mozilla.components.browser.state.selector.findCustomTab
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.feature.pwa.feature.SiteControlsBuilder
import mozilla.components.feature.session.SessionUseCases
import org.mozilla.fenix.R

class WebAppSiteControlsBuilder(
    private val store: BrowserStore,
    reloadUrlUseCase: SessionUseCases.ReloadUrlUseCase,
    private val sessionId: String,
    private val manifest: WebAppManifest,
) : SiteControlsBuilder {

    private val inner = SiteControlsBuilder.CopyAndRefresh(reloadUrlUseCase)

    override fun buildNotification(context: Context, builder: Notification.Builder) {
        inner.buildNotification(context, builder)

        if (store.state.findCustomTab(sessionId)?.content?.private != true) {
            return
        }

        builder.setSmallIcon(R.drawable.ic_private_browsing)
        builder.setContentTitle(context.getString(R.string.pwa_site_controls_title_private, manifest.name))
    }

    override fun getFilter() = inner.getFilter()

    override fun onReceiveBroadcast(context: Context, tab: CustomTabSessionState, intent: Intent) {
        inner.onReceiveBroadcast(context, tab, intent)
    }
}
