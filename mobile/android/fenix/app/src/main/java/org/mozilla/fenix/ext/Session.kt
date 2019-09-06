/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.content.Context
import mozilla.components.browser.session.Session
import mozilla.components.feature.media.state.MediaState
import org.mozilla.fenix.home.sessioncontrol.Tab

fun Session.toTab(context: Context, selected: Boolean? = null, mediaState: MediaState? = null): Tab {
    return Tab(
        this.id,
        this.url,
        this.url.urlToTrimmedHost(context),
        this.title,
        selected,
        mediaState
    )
}
