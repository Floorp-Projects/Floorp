/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.session.bundling.SessionBundleStorage

suspend fun SessionBundleStorage.archive(sessionManager: SessionManager) {
    withContext(Dispatchers.IO) {
        save(sessionManager.createSnapshot())
        launch(Dispatchers.Main) {
            sessionManager.sessions.filter { !it.private }.forEach {
                sessionManager.remove(it)
            }
        }
        new()
    }
}
