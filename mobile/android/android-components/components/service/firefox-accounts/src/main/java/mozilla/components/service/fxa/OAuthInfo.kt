/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import com.sun.jna.Pointer
import com.sun.jna.Structure

import java.util.Arrays

class OAuthInfo internal constructor(raw: Raw) {

    @JvmField val accessToken: String?
    @JvmField val keys: String?
    @JvmField val scope: String?

    @Suppress("VariableNaming")
    class Raw(p: Pointer) : Structure(p) {
        @JvmField var access_token: Pointer? = null
        @JvmField var keys: Pointer? = null
        @JvmField var scope: Pointer? = null

        init {
            read()
        }

        override fun getFieldOrder(): List<String> {
            return Arrays.asList("access_token", "keys", "scope")
        }
    }

    init {
        this.accessToken = raw.access_token?.getString(0, RustObject.RUST_STRING_ENCODING)
        this.keys = raw.keys?.getString(0, RustObject.RUST_STRING_ENCODING)
        this.scope = raw.scope?.getString(0, RustObject.RUST_STRING_ENCODING)
        FxaClient.INSTANCE.fxa_oauth_info_free(raw.pointer)
    }
}
