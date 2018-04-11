/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.engine

import android.content.Context
import android.util.AttributeSet

/**
 * Entry point for interacting with the engine implementation.
 */
interface Engine {
    /**
     * Create a new view for rendering web content.
     */
    fun createView(context: Context, attrs: AttributeSet? = null): EngineView

    /**
     * Create a new engine session.
     */
    fun createSession(): EngineSession
}
