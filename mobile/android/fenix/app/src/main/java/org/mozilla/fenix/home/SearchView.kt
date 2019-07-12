/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.content.Context
import android.graphics.drawable.TransitionDrawable
import android.util.AttributeSet
import android.widget.FrameLayout
import org.mozilla.fenix.R

class SearchView(context: Context, attrs: AttributeSet) : FrameLayout(context, attrs) {
    private val lightDrawable =
        resources.getDrawable(R.drawable.home_search_background_normal, context.theme)
    private val darkDrawable =
        resources.getDrawable(R.drawable.home_search_background_dark, context.theme)
    private val darkNoBorderDrawable =
        resources.getDrawable(R.drawable.home_search_background_dark_no_border, context.theme)

    private val lightToDark = TransitionDrawable(arrayOf(lightDrawable, darkDrawable))
    private val darkToNoBorder = TransitionDrawable(arrayOf(darkDrawable, darkNoBorderDrawable))

    fun transitionToLight() {
        background = lightToDark
        lightToDark.reverseTransition(transitionDurationMs)
    }

    fun transitionToDark() {
        background = lightToDark
        lightToDark.startTransition(transitionDurationMs)
    }

    fun transitionToDarkFromNoBorder() {
        background = darkToNoBorder
        darkToNoBorder.reverseTransition(transitionDurationMs)
    }

    fun transitionToDarkNoBorder() {
        background = darkToNoBorder
        darkToNoBorder.startTransition(transitionDurationMs)
    }

    companion object {
        const val transitionDurationMs = 200
    }
}
