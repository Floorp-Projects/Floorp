/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import com.sun.jna.Pointer
import com.sun.jna.Structure

import java.util.Arrays

class SyncKeys internal constructor(raw: Raw) {

    @JvmField val syncKey: String?
    @JvmField val xcs: String?

    @Suppress("VariableNaming")
    class Raw(p: Pointer) : Structure(p) {
        @JvmField var sync_key: Pointer? = null
        @JvmField var xcs: Pointer? = null

        init {
            read()
        }

        override fun getFieldOrder(): List<String> {
            return Arrays.asList("sync_key", "xcs")
        }
    }

    init {
        this.syncKey = raw.sync_key?.getString(0, RustObject.RUST_STRING_ENCODING)
        this.xcs = raw.xcs?.getString(0, RustObject.RUST_STRING_ENCODING)
        FxaClient.INSTANCE.fxa_sync_keys_free(raw.pointer)
    }
}
