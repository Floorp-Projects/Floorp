/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import mozilla.components.engine.EngineSession
import mozilla.components.session.Session

/**
 * Proxy class that will subscribe to an EngineSession and update the Session object whenever new
 * data is available.
 */
class SessionProxy(
    private val session: Session,
    engineSession: EngineSession
) : EngineSession.Observer {
    init {
        engineSession.register(this)
    }

    override fun onLocationChange(url: String) {
        session.url = url
    }
}
