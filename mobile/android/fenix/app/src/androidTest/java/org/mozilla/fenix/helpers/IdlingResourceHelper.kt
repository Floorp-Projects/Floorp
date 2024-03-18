/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("DEPRECATION")

package org.mozilla.fenix.helpers

import android.util.Log
import androidx.test.espresso.IdlingRegistry
import org.mozilla.fenix.helpers.Constants.TAG

object IdlingResourceHelper {
    fun unregisterAllIdlingResources() {
        for (resource in IdlingRegistry.getInstance().resources) {
            Log.i(TAG, "unregisterAllIdlingResources: Trying to unregister ${resource.name} resource")
            IdlingRegistry.getInstance().unregister(resource)
            Log.i(TAG, "unregisterAllIdlingResources: Unregistered ${resource.name} resource")
        }
    }
}
