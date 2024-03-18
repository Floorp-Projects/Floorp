/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.feature

import android.app.Activity
import android.content.Intent

/**
 * Generic interface for fragments, activities, features and other components that want to handle
 * the [Activity.onActivityResult] event.
 */
interface ActivityResultHandler {

    /**
     * Called when the activity we launched exists and we may want to handle the result.
     *
     * @return true if the result was consumed and no other component needs to be notified.
     */
    fun onActivityResult(requestCode: Int, data: Intent?, resultCode: Int): Boolean
}
