/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.content.Context
import mozilla.components.browser.session.Session
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.MediaState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import org.mozilla.fenix.home.Tab

fun Session.toTab(context: Context, selected: Boolean? = null): Tab =
    this.toTab(
        context.components.core.store,
        context.components.publicSuffixList,
        selected
    )

fun Session.toTab(store: BrowserStore, publicSuffixList: PublicSuffixList, selected: Boolean? = null): Tab {
    val url = store.state.findTab(this.id)?.readerState?.activeUrl ?: this.url
    return Tab(
        sessionId = this.id,
        url = url,
        hostname = url.toShortUrl(publicSuffixList),
        title = this.title,
        selected = selected,
        icon = this.icon,
        mediaState = getMediaStateForSession(store, this)
    )
}

private fun getMediaStateForSession(store: BrowserStore, session: Session): MediaState.State {
    // For now we are looking up the media state for this session in the BrowserStore. Eventually
    // we will migrate away from Session(Manager) and can use BrowserStore and BrowserState directly.
    return if (store.state.media.aggregate.activeTabId == session.id) {
        store.state.media.aggregate.state
    } else {
        MediaState.State.NONE
    }
}
