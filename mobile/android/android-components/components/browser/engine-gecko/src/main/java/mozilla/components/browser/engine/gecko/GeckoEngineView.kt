/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import android.content.Context
import android.graphics.Bitmap
import android.support.v4.view.ViewCompat
import android.util.AttributeSet
import android.widget.FrameLayout
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView

/**
 * Gecko-based EngineView implementation.
 */
class GeckoEngineView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : FrameLayout(context, attrs, defStyleAttr), EngineView {

    internal var currentGeckoView = object : NestedGeckoView(context) {
        override fun onDetachedFromWindow() {
            // We are releasing the session before GeckoView gets detached from the window. Otherwise
            // GeckoView will close the session automatically and we do not want that.
            releaseSession()

            super.onDetachedFromWindow()
        }
    }.apply {
        // Explicitly mark this view as important for autofill. The default "auto" doesn't seem to trigger any
        // autofill behavior for us here.
        ViewCompat.setImportantForAutofill(this, 0x1 /* View.IMPORTANT_FOR_AUTOFILL_YES */)
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

    override fun canScrollVerticallyDown() = true // waiting for this issue https://bugzilla.mozilla.org/show_bug.cgi?id=1507569

    override fun captureThumbnail(onFinish: (Bitmap?) -> Unit) = Unit
}
