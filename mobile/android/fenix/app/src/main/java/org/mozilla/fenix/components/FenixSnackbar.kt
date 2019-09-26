/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.util.TypedValue
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.content.ContextCompat
import androidx.core.widget.TextViewCompat
import com.google.android.material.snackbar.BaseTransientBottomBar
import com.google.android.material.snackbar.ContentViewCallback
import com.google.android.material.snackbar.Snackbar
import kotlinx.android.synthetic.main.fenix_snackbar.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.test.Mockable

@Mockable
class FenixSnackbar private constructor(
    parent: ViewGroup,
    content: View,
    contentViewCallback: FenixSnackbarCallback,
    isError: Boolean
) : BaseTransientBottomBar<FenixSnackbar>(parent, content, contentViewCallback) {

    init {
        view.background = null

        view.snackbar_layout.background = if (isError) {
            ContextCompat.getDrawable(context, R.drawable.fenix_snackbar_error_background)
        } else {
            ContextCompat.getDrawable(context, R.drawable.fenix_snackbar_background)
        }

        content.snackbar_btn.increaseTapArea(actionButtonIncreaseDps)

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
        const val LENGTH_LONG = Snackbar.LENGTH_LONG
        const val LENGTH_SHORT = Snackbar.LENGTH_SHORT
        const val LENGTH_INDEFINITE = Snackbar.LENGTH_INDEFINITE

        private const val minTextSize = 12
        private const val maxTextSize = 18
        private const val actionButtonIncreaseDps = 16
        private const val stepGranularity = 1

        fun make(view: View, duration: Int, isError: Boolean = false): FenixSnackbar {
            val parent = findSuitableParent(view) ?: run {
                throw IllegalArgumentException(
                    "No suitable parent found from the given view. Please provide a valid view."
                )
            }

            val inflater = LayoutInflater.from(parent.context)
            val content = inflater.inflate(R.layout.fenix_snackbar, parent, false)

            val callback = FenixSnackbarCallback(content)
            return FenixSnackbar(parent, content, callback, isError).also {
                it.duration = duration
            }
        }

        // Use the same implementation of `Snackbar`
        private fun findSuitableParent(_view: View?): ViewGroup? {
            var view = _view
            var fallback: ViewGroup? = null

            do {
                if (view is CoordinatorLayout) {
                    return view
                }

                if (view is FrameLayout) {
                    if (view.id == android.R.id.content) {
                        return view
                    }

                    fallback = view
                }

                if (view != null) {
                    val parent = view.parent
                    view = if (parent is View) parent else null
                }
            } while (view != null)

            return fallback
        }
    }
}

private class FenixSnackbarCallback(
    private val content: View
) : ContentViewCallback {

    override fun animateContentIn(delay: Int, duration: Int) {
        content.translationY = (content.height).toFloat()
        content.animate().apply {
            translationY(defaultYTranslation)
            setDuration(animateInDuration)
            startDelay = delay.toLong()
        }
    }

    override fun animateContentOut(delay: Int, duration: Int) {
        content.translationY = defaultYTranslation
        content.animate().apply {
            translationY((content.height).toFloat())
            setDuration(animateOutDuration)
            startDelay = delay.toLong()
        }
    }

    companion object {
        private const val defaultYTranslation = 0f
        private const val animateInDuration = 200L
        private const val animateOutDuration = 150L
    }
}

class FenixSnackbarPresenter(
    private val view: View
) {
    fun present(
        text: String,
        length: Int = FenixSnackbar.LENGTH_LONG,
        action: (() -> Unit)? = null,
        actionName: String? = null,
        isError: Boolean = false
    ) {
        FenixSnackbar.make(view, length, isError).setText(text).let {
            if (action != null && actionName != null) it.setAction(actionName, action) else it
        }.show()
    }
}
