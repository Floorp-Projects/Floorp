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
     * Describes the different types of window requests.
     */
    enum class Type { OPEN, CLOSE }

    /**
     * The [Type] of this window request, indicating whether to open or
     * close a window.
     */
    val type: Type

    /**
     * The URL which should be opened in a new window. May be
     * empty if the request was created from JavaScript (using
     * window.open()).
     */
    val url: String

    /**
     * Prepares an [EngineSession] for the window request. This is used to
     * attach state (e.g. a native session or view) to the engine session.
     *
     * @return the prepared and ready-to-use [EngineSession].
     */
    fun prepare(): EngineSession

    /**
     * Starts the window request.
     */
    fun start() = Unit
}
