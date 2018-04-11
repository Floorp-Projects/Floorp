/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.engine

import android.view.View

/**
 * View component that renders web content.
 */
interface EngineView {

    /**
     * Convenience method to cast the implementation of this interface to an Android View object.
     */
    fun asView(): View = this as View

    /**
     * Render the content of the given session.
     */
    fun render(session: EngineSession)
}
