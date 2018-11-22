/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.window

import mozilla.components.concept.engine.EngineSession

/**
 * Represents a request to open or close a browser window.
 */
interface WindowRequest {

    /**
     * The URL which should be opened in a new window. May be
     * empty if the request was created from JavaScript (using
     * window.open()).
     */
    val url: String

    /**
     * Prepares the provided [EngineSession] for the window request. This
     * is used to attach state (e.g. a native session) to the engine session.
     *
     * @param engineSession the engine session to prepare.
     */
    fun prepare(engineSession: EngineSession)

    /**
     * Starts the window request.
     */
    fun start()
}
