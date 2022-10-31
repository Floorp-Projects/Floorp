/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs.menu

import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import androidx.core.net.toUri
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.concept.menu.candidate.TextMenuCandidate

/**
 * Build menu items displayed when the 3-dot overflow menu is opened.
 * These items are provided by the app that creates the custom tab,
 * and should be inserted alongside menu items created by the browser.
 */
fun CustomTabSessionState.createCustomTabMenuCandidates(context: Context) =
    config.menuItems.map { item ->
        TextMenuCandidate(
            text = item.name,
        ) {
            item.pendingIntent.sendWithUrl(context, content.url)
        }
    }

internal fun PendingIntent.sendWithUrl(context: Context, url: String) = send(
    context,
    0,
    Intent(null, url.toUri()),
)
