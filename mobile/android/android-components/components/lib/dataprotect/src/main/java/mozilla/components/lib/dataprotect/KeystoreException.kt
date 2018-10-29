/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.dataprotect

/**
 * Exception type thrown by {@link Keystore} when an error is encountered.
 *
 */
class KeystoreException(
    message: String? = null,
    cause: Throwable? = null
) : Exception(message, cause)
