/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import mozilla.appservices.fxaclient.Profile as InternalProfile

data class Avatar(
    val url: String,
    val isDefault: Boolean
)

data class Profile(
    val uid: String?,
    val email: String?,
    val avatar: Avatar?,
    val displayName: String?
) {
    companion object {
        internal fun fromInternal(p: InternalProfile): Profile {
            return Profile(
                uid = p.uid,
                email = p.email,
                avatar = p.avatar?.let {
                    Avatar(
                        url = it,
                        isDefault = p.avatarDefault
                    )
                },
                displayName = p.displayName
            )
        }
    }
}
