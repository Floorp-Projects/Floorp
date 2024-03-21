/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customtabs

import android.content.Context
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.toolbar.ScrollableToolbar
import mozilla.components.feature.toolbar.ToolbarFeature
import org.mozilla.fenix.components.toolbar.ToolbarIntegration
import org.mozilla.fenix.components.toolbar.ToolbarMenu

class CustomTabToolbarIntegration(
    context: Context,
    toolbar: BrowserToolbar,
    scrollableToolbar: ScrollableToolbar,
    toolbarMenu: ToolbarMenu,
    sessionId: String,
    isPrivate: Boolean,
) : ToolbarIntegration(
    context = context,
    toolbar = toolbar,
    scrollableToolbar = scrollableToolbar,
    toolbarMenu = toolbarMenu,
    sessionId = sessionId,
    isPrivate = isPrivate,
    renderStyle = ToolbarFeature.RenderStyle.RegistrableDomain,
)
