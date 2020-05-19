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
    // Creating a non-private Session as a workaround here. This needs to be an actual private
    // session before we can land this. With a private session we are currently not able to save
    // any tracking protection exceptions, see:
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1644156
    // As a workaround we are currently removing all data manually, using Engine.clearData(), once
    // a browsing session ends.
    return Session(url, source = source, private = false)
}
