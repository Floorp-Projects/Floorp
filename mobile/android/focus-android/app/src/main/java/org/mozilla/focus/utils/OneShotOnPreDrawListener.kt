/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils

import android.view.View
import android.view.ViewTreeObserver

/**
 * A OnPreDrawListener implementation that will execute a callback once and then unsubscribe itself.
 */
class OneShotOnPreDrawListener(
    private val view: View,
    private inline val onPreDraw: (view: View) -> Boolean
) : ViewTreeObserver.OnPreDrawListener {

    override fun onPreDraw(): Boolean {
        view.viewTreeObserver.removeOnPreDrawListener(this)

        return onPreDraw(view)
    }
}
