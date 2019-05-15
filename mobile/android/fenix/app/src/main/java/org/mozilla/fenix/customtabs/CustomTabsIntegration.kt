/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customtabs

import android.app.Activity
import android.content.Context
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.feature.customtabs.CustomTabsToolbarFeature
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.fenix.components.toolbar.ToolbarMenu

class CustomTabsIntegration(
    context: Context,
    sessionManager: SessionManager,
    val toolbar: BrowserToolbar,
    sessionId: String,
    activity: Activity?,
    onItemTapped: (ToolbarMenu.Item) -> Unit = {}
) : LifecycleAwareFeature, BackHandler {

    init {
        toolbar.elevation = 0f
    }

    private val customTabToolbarMenu by lazy {
        CustomTabToolbarMenu(
            context,
            sessionManager,
            sessionId,
            onItemTapped = onItemTapped
        )
    }

    private val feature = CustomTabsToolbarFeature(
        sessionManager,
        toolbar,
        sessionId,
        customTabToolbarMenu.menuBuilder,
        START_OF_MENU_ITEMS_INDEX,
        closeListener = { activity?.finish() })

    override fun start() {
        feature.start()
    }

    override fun stop() {
        feature.stop()
    }

    override fun onBackPressed(): Boolean {
        return feature.onBackPressed()
    }

    companion object {
        const val START_OF_MENU_ITEMS_INDEX = 2
    }
}
