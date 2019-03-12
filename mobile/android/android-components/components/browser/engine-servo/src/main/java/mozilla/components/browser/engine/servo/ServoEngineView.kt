/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.servo

import android.content.Context
import android.graphics.Bitmap
import android.util.AttributeSet
import android.widget.FrameLayout
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import org.mozilla.servoview.ServoView

/**
 * Servo-based implementation of EngineView.
 */
class ServoEngineView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : FrameLayout(context, attrs, defStyleAttr), EngineView {
    internal val servoView: ServoView = ServoView(context, attrs).apply {
        addView(this, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT))
    }

    override fun render(session: EngineSession) {
        if (session !is ServoEngineSession) {
            throw IllegalArgumentException("Not a ServoEngineSession")
        }

        session.attachView(this)
    }

    override fun onResume() {
        super.onResume()

        servoView.onResume()
    }

    override fun onPause() {
        super.onPause()

        servoView.onPause()
    }

    override fun captureThumbnail(onFinish: (Bitmap?) -> Unit) = Unit
}
