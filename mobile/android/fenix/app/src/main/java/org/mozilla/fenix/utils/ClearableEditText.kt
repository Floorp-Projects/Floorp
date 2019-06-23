/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.PorterDuff.Mode.SRC_IN
import android.graphics.PorterDuffColorFilter
import android.graphics.drawable.Drawable
import android.util.AttributeSet
import android.view.MotionEvent
import androidx.appcompat.widget.AppCompatEditText
import androidx.core.content.ContextCompat.getColor
import org.mozilla.fenix.R
import org.mozilla.fenix.ThemeManager

class ClearableEditText @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = R.attr.editTextStyle
) :
    AppCompatEditText(context, attrs, defStyleAttr) {

    @SuppressLint("ClickableViewAccessibility")
    override fun onTouchEvent(event: MotionEvent?): Boolean {
        if (length() != 0 && event?.action == MotionEvent.ACTION_UP &&
            event.rawX >= (this@ClearableEditText.right - this@ClearableEditText.compoundPaddingRight)
        ) {
            this@ClearableEditText.setText("")
            return true
        }
        return super.onTouchEvent(event)
    }

    override fun onTextChanged(text: CharSequence?, start: Int, lengthBefore: Int, lengthAfter: Int) {
        super.onTextChanged(text, start, lengthBefore, lengthAfter)
        if (lengthAfter != 0 && error == null) {
            setCompoundDrawablesWithIntrinsicBounds(0, 0, R.drawable.ic_clear, 0)
            for (drawable: Drawable in compoundDrawables.filterNotNull()) {
                val color = ThemeManager.resolveAttribute(R.attr.primaryText, context!!)
                drawable.colorFilter = PorterDuffColorFilter(getColor(context, color), SRC_IN)
            }
        } else {
            setCompoundDrawablesWithIntrinsicBounds(0, 0, 0, 0)
        }
    }
}
