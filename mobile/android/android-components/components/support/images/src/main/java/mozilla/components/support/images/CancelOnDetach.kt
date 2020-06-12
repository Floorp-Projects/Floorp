/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.images

import android.view.View
import kotlinx.coroutines.Job

/**
 * Cancels the provided job when a view is detached from the window
 */
class CancelOnDetach(private val job: Job) : View.OnAttachStateChangeListener {

    override fun onViewAttachedToWindow(v: View?) = Unit

    override fun onViewDetachedFromWindow(v: View?) = job.cancel()
}
