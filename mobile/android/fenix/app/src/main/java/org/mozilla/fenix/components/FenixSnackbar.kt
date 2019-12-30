/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.util.TypedValue
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.appcompat.content.res.AppCompatResources
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.widget.TextViewCompat
import com.google.android.material.snackbar.BaseTransientBottomBar
import com.google.android.material.snackbar.ContentViewCallback
import com.google.android.material.snackbar.Snackbar
import kotlinx.android.synthetic.main.fenix_snackbar.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.ext.settings
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
            AppCompatResources.getDrawable(context, R.drawable.fenix_snackbar_error_background)
        } else {
            AppCompatResources.getDrawable(context, R.drawable.fenix_snackbar_background)
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

/**
 * This snackbar presenter should be used when displaying a snackbar that will appear in
 * the BrowserFragment as it takes into account the position of the BrowserToolbar
 */
class BrowserSnackbarPresenter(
    private val view: View
) {
    fun present(
        text: String,
        length: Int = FenixSnackbar.LENGTH_LONG,
        action: (() -> Unit)? = null,
        actionName: String? = null,
        isError: Boolean = false
    ) {
        val shouldUseBottomToolbar = view.context.settings().shouldUseBottomToolbar
        val toolbarHeight = view.context.resources
            .getDimensionPixelSize(R.dimen.browser_toolbar_height)

        FenixSnackbar.make(view, length, isError).apply {
            if (action != null && actionName != null) setAction(actionName, action)
            setText(text)
            view.setPadding(
                0,
                0,
                0,
                if (shouldUseBottomToolbar) toolbarHeight else 0
            )
            show()
        }
    }
}
