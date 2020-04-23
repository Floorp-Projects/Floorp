/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.sitepermissions

import androidx.core.net.toUri
import org.mozilla.fenix.components.Components

/**
 * Try to reload a session if a session with the given [origin] it is found.
 * @param origin The origin the session to be reloaded.
 */
internal fun Components.tryReloadTabBy(origin: String) {
    val session = core.sessionManager.all.find { it.url.toUri().host == origin }
    useCases.sessionUseCases.reload(session)
}
