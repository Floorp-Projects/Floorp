/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.serviceworker

import mozilla.components.concept.engine.EngineSession

/**
 * Application delegate for handling all service worker requests.
 */
interface ServiceWorkerDelegate {
    /**
     * Handles requests to open a new tab using the provided [engineSession].
     * Implementations should not try to load any url, this will be executed by the service worker
     * through the [engineSession].
     *
     * @param engineSession New [EngineSession] in which a service worker will try to load a specific url.
     *
     * @return
     * - `true` when a new tab is created and a service worker is allowed to open an url in it,
     * - `false` otherwise.
     */
    fun addNewTab(engineSession: EngineSession): Boolean
}
