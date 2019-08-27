/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import mozilla.components.browser.session.SessionManager

/**
 * Returns only normal or only private sessions.
 */
fun SessionManager.sessionsOfType(private: Boolean) =
    sessions.asSequence().filter { it.private == private }
