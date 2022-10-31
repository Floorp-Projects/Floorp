/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.cfr.helper

import android.view.View

/**
 * Simpler [View.OnAttachStateChangeListener] only informing about
 * [View.OnAttachStateChangeListener.onViewDetachedFromWindow].
 */
internal class ViewDetachedListener(val onDismiss: () -> Unit) : View.OnAttachStateChangeListener {
    override fun onViewAttachedToWindow(v: View?) = Unit

    override fun onViewDetachedFromWindow(v: View?) {
        onDismiss()
    }
}
