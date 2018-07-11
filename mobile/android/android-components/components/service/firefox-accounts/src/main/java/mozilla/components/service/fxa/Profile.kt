/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import com.sun.jna.Pointer
import com.sun.jna.Structure

import java.util.Arrays

class Profile internal constructor(raw: Raw) {

    @JvmField val uid: String?
    @JvmField val email: String?
    @JvmField val avatar: String?
    @JvmField val displayName: String?

    @Suppress("VariableNaming")
    class Raw(p: Pointer) : Structure(p) {
        @JvmField var uid: String? = null
        @JvmField var email: String? = null
        @JvmField var avatar: String? = null
        @JvmField var display_name: String? = null

        init {
            read()
        }

        override fun getFieldOrder(): List<String> {
            return Arrays.asList("uid", "email", "avatar", "display_name")
        }
    }

    init {
        this.uid = raw.uid
        this.email = raw.email
        this.avatar = raw.avatar
        this.displayName = raw.display_name
        FxaClient.INSTANCE.fxa_profile_free(raw.pointer)
    }
}
