/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.browser.state.ext
import mozilla.components.concept.engine.permission.PermissionRequest

/**
 * @returns true if the given [permissionRequest] is contained in this list otherwise false
 * */
fun List<PermissionRequest>.containsPermission(permissionRequest: PermissionRequest): Boolean {
    return this.any {
        it.uri == permissionRequest.uri &&
            it.permissions == permissionRequest.permissions
    }
}
