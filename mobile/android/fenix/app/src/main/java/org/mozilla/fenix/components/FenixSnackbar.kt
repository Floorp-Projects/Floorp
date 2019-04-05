/* This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.util.TypedValue
import android.view.View
import android.view.ViewGroup
import com.google.android.material.snackbar.BaseTransientBottomBar
import android.view.LayoutInflater
import androidx.core.widget.TextViewCompat
import kotlinx.android.synthetic.main.fenix_snackbar.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.increaseTapArea

class FenixSnackbar private constructor(
    parent: ViewGroup,
    content: View,
    contentViewCallback: FenixSnackbarCallback
) : BaseTransientBottomBar<FenixSnackbar>(parent, content, contentViewCallback) {

    init {
        view.background = null
        content.snackbar_btn.increaseTapArea(increaseTouchableAreaBy)

        TextViewCompat.setAutoSizeTextTypeUniformWithConfiguration(
            content.snackbar_text,
            minTextSize,
            maxTextSize,
            stepGranularity,
            TypedValue.COMPLEX_UNIT_SP
        )
    }

    fun setText(text: String) = this.apply {
        view.snackbar_text.text = text
    }

    fun setAction(text: String, action: () -> Unit) = this.apply {
        view.snackbar_btn.apply {
            setText(text)
            visibility = View.VISIBLE
            setOnClickListener {
                action.invoke()
                dismiss()
            }
        }
    }

    companion object {
        private const val minTextSize = 12
        private const val maxTextSize = 18
        private const val increaseTouchableAreaBy = 16
        private const val stepGranularity = 1

        fun make(parent: ViewGroup, duration: Int): FenixSnackbar {
            val inflater = LayoutInflater.from(parent.context)
            val content = inflater.inflate(R.layout.fenix_snackbar, parent, false)

            // create snackbar with custom view
            val callback = FenixSnackbarCallback(content)
            val fenixSnackbar = FenixSnackbar(parent, content, callback)

            // set snackbar duration
            fenixSnackbar.duration = duration
            return fenixSnackbar
        }
    }
}

private class FenixSnackbarCallback(
    private val content: View
) : com.google.android.material.snackbar.ContentViewCallback {

    override fun animateContentIn(delay: Int, duration: Int) {
        content.scaleY = minScaleY
        content.animate().apply {
            scaleY(maxScaleY)
            setDuration(duration.toLong())
            startDelay = delay.toLong()
        }
    }

    override fun animateContentOut(delay: Int, duration: Int) {
        content.scaleY = maxScaleY
        content.animate().apply {
            scaleY(minScaleY)
            setDuration(duration.toLong())
            startDelay = delay.toLong()
        }
    }

    companion object {
        private const val minScaleY = 0f
        private const val maxScaleY = 1f
    }
}
