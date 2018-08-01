/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import android.content.Context
import android.util.AttributeSet
import android.widget.FrameLayout
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import org.mozilla.geckoview.GeckoView

/**
 * Gecko-based EngineView implementation.
 */
class GeckoEngineView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : FrameLayout(context, attrs, defStyleAttr), EngineView {

    internal var currentGeckoView = object : GeckoView(context) {
        override fun onDetachedFromWindow() {
            // We are releasing the session before GeckoView gets detached from the window. Otherwise
            // GeckoView will close the session automatically and we do not want that.
            releaseSession()

            super.onDetachedFromWindow()
        }
    }

    init {
        // Currently this is just a FrameLayout with a single GeckoView instance. Eventually this
        // implementation should handle at least two GeckoView so that we can switch between
        addView(currentGeckoView)
    }

    /**
     * Render the content of the given session.
     */
    override fun render(session: EngineSession) {
        val internalSession = session as GeckoEngineSession

        if (currentGeckoView.session != internalSession.geckoSession) {
            currentGeckoView.session?.let {
                // Release a previously assigned session. Otherwise GeckoView will close it
                // automatically.
                currentGeckoView.releaseSession()
            }

            currentGeckoView.session = internalSession.geckoSession
        }
    }
}
