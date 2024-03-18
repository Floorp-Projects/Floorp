/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

/**
 * Converts a string to an enum value, ignoring case and replacing spaces with underscores.
 * If the string does not match any of the enum values, the default value is returned.
 */
inline fun <reified T : Enum<T>> String.asEnumOrDefault(defaultValue: T? = null): T? =
    enumValues<T>().firstOrNull { it.name.equals(this.replace(" ", "_"), ignoreCase = true) } ?: defaultValue
