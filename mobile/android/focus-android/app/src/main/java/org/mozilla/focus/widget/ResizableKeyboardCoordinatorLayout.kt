/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.widget

import android.content.Context
import android.util.AttributeSet
import androidx.coordinatorlayout.widget.CoordinatorLayout

/**
 * A CoordinatorLayout implementation that resizes dynamically based on whether a keyboard is visible or not.
 */
class ResizableKeyboardCoordinatorLayout @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : CoordinatorLayout(context, attrs, defStyleAttr) {
    private val delegate = ResizableKeyboardViewDelegate(this, attrs)

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        delegate.onAttachedToWindow()
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()
        delegate.onDetachedFromWindow()
    }

    override fun requestDisallowInterceptTouchEvent(b: Boolean) {
        // As this is a direct parent of EngineView, we don't want to propagate this request to the parent
        // because that would prevent the hiding of the toolbar.
    }
}
