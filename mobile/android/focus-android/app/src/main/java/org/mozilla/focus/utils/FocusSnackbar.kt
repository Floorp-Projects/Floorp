/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils

import android.content.Context
import android.graphics.Color
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.view.isVisible
import androidx.core.view.updatePadding
import com.google.android.material.snackbar.BaseTransientBottomBar
import com.google.android.material.snackbar.ContentViewCallback
import com.google.android.material.snackbar.Snackbar
import org.mozilla.focus.databinding.FocusSnackbarBinding
import org.mozilla.focus.ext.settings

class FocusSnackbar private constructor(
    parent: ViewGroup,
    private val binding: FocusSnackbarBinding,
    contentViewCallback: FocusSnackbarCallback,
) : BaseTransientBottomBar<FocusSnackbar>(parent, binding.root, contentViewCallback) {

    init {
        view.setBackgroundColor(Color.TRANSPARENT)
        view.setPadding(0, 0, 0, 0)
    }

    fun setText(text: String) = apply {
        binding.snackbarText.text = text
    }

    /**
     * Sets an action to be performed on clicking [FocusSnackbar]'s action button.
     */
    fun setAction(text: String, action: (Context) -> Unit) = apply {
        binding.snackbarAction.apply {
            setText(text)
            isVisible = true
            setOnClickListener {
                action.invoke(it.context)
                dismiss()
            }
        }
    }

    companion object {
        const val LENGTH_LONG = Snackbar.LENGTH_LONG
        const val LENGTH_SHORT = Snackbar.LENGTH_SHORT
        private const val LENGTH_ACCESSIBLE = 15000 // 15 seconds in ms

        /**
         * Display a custom Focus Snackbar in the given view with duration and proper styling.
         */
        fun make(
            view: View,
            duration: Int = LENGTH_LONG,
        ): FocusSnackbar {
            val parent = findSuitableParent(view) ?: run {
                throw IllegalArgumentException(
                    "No suitable parent found from the given view. Please provide a valid view.",
                )
            }

            val inflater = LayoutInflater.from(parent.context)
            val binding = FocusSnackbarBinding.inflate(inflater, parent, false)

            val durationOrAccessibleDuration =
                if (parent.context.settings.isAccessibilityEnabled()) {
                    LENGTH_ACCESSIBLE
                } else {
                    duration
                }

            val callback = FocusSnackbarCallback(binding.root)

            return FocusSnackbar(parent, binding, callback).also {
                it.duration = durationOrAccessibleDuration
                it.view.updatePadding(
                    bottom = 0,
                )
            }
        }

        // Use the same implementation of Android `Snackbar`
        @SuppressWarnings("FunctionParameterNaming")
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

private class FocusSnackbarCallback(
    private val content: View,
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
