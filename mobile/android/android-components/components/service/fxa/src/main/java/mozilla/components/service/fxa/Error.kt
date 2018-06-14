/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import com.sun.jna.Pointer
import com.sun.jna.Structure
import java.util.Arrays

open class Error : Structure() {

    class ByReference : Error(), Structure.ByReference

    @JvmField var code: Int = 0
    @JvmField var message: Pointer? = null

    init {
        read()
    }

    /**
     * Does this represent success?
     */
    fun isSuccess(): Boolean {
        return this.code == 0
    }

    /**
     * Does this represent failure?
     */
    fun isFailure(): Boolean {
        return this.code != 0
    }

    /**
     * Get and consume the error message, or null if there is none.
     */
    fun consumeMessage(): String? {
        val result = this.getMessage()
        if (this.message != null) {
            FxaClient.INSTANCE.fxa_str_free(this.message!!)
            this.message = null
        }
        return result
    }

    /**
     * Get the error message or null if there is none.
     */
    fun getMessage(): String? {
        return this.message?.getString(0, "utf8")
    }

    override fun getFieldOrder(): List<String> {
        return Arrays.asList("code", "message")
    }
}
