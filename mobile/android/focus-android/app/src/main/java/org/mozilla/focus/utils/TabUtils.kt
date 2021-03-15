/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils

import mozilla.components.browser.session.Session
import mozilla.components.browser.state.state.SessionState

fun createTab(
    url: String,
    source: SessionState.Source = SessionState.Source.NONE
): Session {
    return Session(url, source = source, private = true)
}
