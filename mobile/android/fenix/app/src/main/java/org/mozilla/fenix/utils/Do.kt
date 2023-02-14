/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

object Do {

    /**
     * Indicates to the linter that the following when statement should be exhaustive.
     *
     * @sample Do exhaustive when (bool) {
     *     true -> Unit
     *     false -> Unit
     * }
     */
    inline infix fun <reified T> exhaustive(any: T?) = any
}
