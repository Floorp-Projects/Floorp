/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.kotlin

import org.json.JSONArray

/*
 * Convenience method to convert a JSONArray into a sequence.
 */
fun JSONArray.asSequence(): Sequence<Any> {
    return object : Sequence<Any> {

        override fun iterator() = object : Iterator<Any> {
            val it = (0 until this@asSequence.length()).iterator()

            override fun next(): Any {
                val i = it.next()
                return this@asSequence.get(i)
            }

            override fun hasNext() = it.hasNext()
        }
    }
}
