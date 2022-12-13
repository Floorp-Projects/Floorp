/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.content.Context
import android.graphics.Rect
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.MotionEvent.ACTION_UP
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.navigation.findNavController
import mozilla.components.support.ktx.android.view.findViewInHierarchy
import org.mozilla.fenix.R
import org.mozilla.fenix.search.SearchDialogFragment

/**
 * Parent layout for [HomeFragment], used to dismiss the [SearchDialogFragment] when
 * interacting with elements in the [HomeFragment].
 */
class HomeFragmentLayout @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : CoordinatorLayout(context, attrs, defStyleAttr) {

    /**
     * Returns whether or not the motion event is touching the private browsing button.
     *
     * @param x X coordinate of the event.
     * @param y Y coordinate of the event.
     */
    private fun isTouchingPrivateButton(x: Float, y: Float): Boolean {
        val view = this.findViewInHierarchy {
            it.id == R.id.privateBrowsingButton
        } ?: return false
        val privateButtonRect = Rect()
        view.getHitRect(privateButtonRect)
        return privateButtonRect.contains(x.toInt(), y.toInt())
    }

    override fun onInterceptTouchEvent(ev: MotionEvent?): Boolean {
        val nav = findNavController()

        // If the private button is touched from the [SearchDialogFragment], then it should not be
        // dismissed to allow the user to continue the search.
        if (ev?.action == ACTION_UP &&
            nav.currentDestination?.id == R.id.searchDialogFragment &&
            !isTouchingPrivateButton(ev.x, ev.y)
        ) {
            nav.popBackStack()
        }

        return super.onInterceptTouchEvent(ev)
    }
}
