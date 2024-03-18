/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils.ext

import androidx.activity.result.contract.ActivityResultContracts
import androidx.fragment.app.Fragment

/**
 * Requests permissions and handles results inside a [Fragment].
 */
fun Fragment.requestInPlacePermissions(
    requestKey: String,
    permissionsToRequest: Array<String>,
    onResult: (Map<String, Boolean>) -> Unit,
) {
    requireActivity().activityResultRegistry.register(
        requestKey,
        ActivityResultContracts.RequestMultiplePermissions(),
    ) { permissions ->
        onResult(permissions)
    }.also {
        it.launch(permissionsToRequest)
    }
}
