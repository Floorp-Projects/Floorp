/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.display

import android.content.Context
import android.graphics.Canvas
import android.util.AttributeSet
import android.view.View
import android.widget.ImageView
import androidx.constraintlayout.widget.ConstraintLayout
import mozilla.components.browser.toolbar.R

/**
 * Custom ConstraintLayout for DisplayToolbar that allows us to draw ripple backgrounds on the toolbar
 * by setting a background to transparent.
 */
class DisplayToolbarView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : ConstraintLayout(context, attrs, defStyleAttr) {
    init {
        // Forcing transparent background so that draw methods will get called and ripple effect
        // for children will be drawn on this layout.
        setBackgroundColor(0x00000000)
    }

    lateinit var backgroundView: ImageView

    override fun onFinishInflate() {
        backgroundView = findViewById(R.id.mozac_browser_toolbar_background)
        backgroundView.visibility = View.INVISIBLE

        super.onFinishInflate()
    }

    // Overriding draw instead of onDraw since we want to draw the background before the actual
    // (transparent) background (with a ripple effect) is drawn.
    override fun draw(canvas: Canvas) {
        canvas.save()
        canvas.translate(backgroundView.x, backgroundView.y)

        backgroundView.drawable?.draw(canvas)

        canvas.restore()

        super.draw(canvas)
    }
}
