/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

/**
 * Converts a string of hexadecimal characters to a Long color value.
 */
fun String.toHexColor() = toLong(radix = 16)
