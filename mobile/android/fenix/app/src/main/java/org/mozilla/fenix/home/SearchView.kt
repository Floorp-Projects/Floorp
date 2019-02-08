/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.content.Context
import android.graphics.drawable.TransitionDrawable
import android.util.AttributeSet
import android.widget.FrameLayout
import org.mozilla.fenix.R

class SearchView(context: Context, attrs: AttributeSet) : FrameLayout(context, attrs) {

    var isPrivateModeEnabled = false

    private val lightDrawable = resources.getDrawable(R.drawable.home_search_background_light)
    private val privateLightDrawable = resources.getDrawable(R.drawable.home_search_background_private)
    private val darkDrawable = resources.getDrawable(R.drawable.home_search_background_dark)
    private val privateDarkDrawable = resources.getDrawable(R.drawable.home_search_background_private_dark)
    private val darkNoBorderDrawable = resources.getDrawable(R.drawable.home_search_background_dark_no_border)
    private val privateDarkNoBorderDrawable =
        resources.getDrawable(R.drawable.home_search_background_private_dark_no_border)

    private val lightToDark = TransitionDrawable(arrayOf(lightDrawable, darkDrawable))
    private val darkToNoBorder = TransitionDrawable(arrayOf(darkDrawable, darkNoBorderDrawable))
    private val privateLightToDark = TransitionDrawable(arrayOf(privateLightDrawable, privateDarkDrawable))
    private val privateDarkToNoBorder = TransitionDrawable(arrayOf(privateDarkDrawable, privateDarkNoBorderDrawable))

    fun transitionToLight() {
        if (isPrivateModeEnabled) {
            background = privateLightToDark
            privateLightToDark.reverseTransition(transitionDurationMs)
        } else {
            background = lightToDark
            lightToDark.reverseTransition(transitionDurationMs)
        }
    }

    fun transitionToDark() {
        if (isPrivateModeEnabled) {
            background = privateLightToDark
            privateLightToDark.startTransition(transitionDurationMs)
        } else {
            background = lightToDark
            lightToDark.startTransition(transitionDurationMs)
        }
    }

    fun transitionToDarkFromNoBorder() {
        if (isPrivateModeEnabled) {
            background = privateDarkToNoBorder
            privateDarkToNoBorder.reverseTransition(transitionDurationMs)
        } else {
            background = darkToNoBorder
            darkToNoBorder.reverseTransition(transitionDurationMs)
        }
    }

    fun transitionToDarkNoBorder() {
        if (isPrivateModeEnabled) {
            background = privateDarkToNoBorder
            privateDarkToNoBorder.startTransition(transitionDurationMs)
        } else {
            background = darkToNoBorder
            darkToNoBorder.startTransition(transitionDurationMs)
        }
    }

    companion object {
        const val transitionDurationMs = 500
    }
}
