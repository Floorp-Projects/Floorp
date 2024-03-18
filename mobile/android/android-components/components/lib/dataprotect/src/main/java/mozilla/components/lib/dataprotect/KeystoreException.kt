/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.dataprotect

import java.security.GeneralSecurityException

/**
 * Exception type thrown by {@link Keystore} when an error is encountered that
 * is not otherwise covered by an existing sub-class to `GeneralSecurityException`.
 *
 */
class KeystoreException(
    message: String? = null,
    cause: Throwable? = null,
) : GeneralSecurityException(message, cause)
