/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.test.fakes.engine

import android.content.Context
import android.graphics.Bitmap
import android.view.View
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.selection.SelectionActionDelegate

/**
 * A fake [EngineView] to be used in tests.
 */
class FakeEngineView(context: Context) : View(context), EngineView {
    override fun render(session: EngineSession) = Unit

    override fun captureThumbnail(onFinish: (Bitmap?) -> Unit) = Unit

    override fun clearSelection() = Unit

    override fun setVerticalClipping(clippingHeight: Int) = Unit

    override fun setDynamicToolbarMaxHeight(height: Int) = Unit

    override fun release() = Unit

    override var selectionActionDelegate: SelectionActionDelegate? = null
}
