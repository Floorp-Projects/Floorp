/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.test.file

import org.junit.Assert

/**
 * Loads a file from the resources folder and returns its content as a string object.
 * @param path The path where the file is located
 */
fun Any.loadResourceAsString(path: String): String {
    return javaClass.getResourceAsStream(path)!!.bufferedReader().use {
        it.readText()
    }.also {
        Assert.assertNotNull(it)
    }
}
