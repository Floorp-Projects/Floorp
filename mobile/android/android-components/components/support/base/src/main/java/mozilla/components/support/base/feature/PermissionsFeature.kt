/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.feature

typealias OnNeedToRequestPermissions = (permissions: Array<String>) -> Unit

/**
 * Interface for features that need to request permissions from the user.
 *
 * Example integration:
 *
 * ```
 * class MyFragment : Fragment {
 *     val myFeature = MyPermissionsFeature(
 *         onNeedToRequestPermissions = { permissions ->
 *             requestPermissions(permissions, REQUEST_CODE_MY_FEATURE)
 *         }
 *     )
 *
 *     override fun onRequestPermissionsResult(resultCode: Int, permissions: Array<String>, grantResults: IntArray) {
 *         if (resultCode == REQUEST_CODE_MY_FEATURE) {
 *             myFeature.onPermissionsResult(permissions, grantResults)
 *         }
 *     }
 *
 *     companion object {
 *         private const val REQUEST_CODE_MY_FEATURE = 1
 *     }
 * }
 * ```
 */
interface PermissionsFeature {

    /**
     * A callback invoked when permissions need to be requested by the feature before
     * it can complete its task. Once the request is completed, [onPermissionsResult]
     * needs to be invoked.
     */
    val onNeedToRequestPermissions: OnNeedToRequestPermissions

    /**
     * Notifies the feature that a permission request was completed.
     * The feature should react to this and complete its task.
     *
     * @param permissions The permissions that were granted.
     * @param grantResults The grant results for the corresponding permission
     */
    fun onPermissionsResult(permissions: Array<String>, grantResults: IntArray)
}
