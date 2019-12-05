/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.content.Context
import mozilla.components.browser.session.Session
import mozilla.components.feature.media.state.MediaState
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import org.mozilla.fenix.home.Tab

fun Session.toTab(context: Context, selected: Boolean? = null, mediaState: MediaState? = null): Tab =
    this.toTab(context.components.publicSuffixList, selected, mediaState)

fun Session.toTab(publicSuffixList: PublicSuffixList, selected: Boolean? = null, mediaState: MediaState? = null): Tab {
    return Tab(
        sessionId = this.id,
        url = this.url,
        hostname = this.url.toShortUrl(publicSuffixList),
        title = this.title,
        selected = selected,
        mediaState = mediaState,
        icon = this.icon
    )
}
