/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.push.ext

import mozilla.components.feature.push.PushConnection
import mozilla.components.support.base.log.logger.Logger

/**
 * Executes the block if the Push Manager is initialized.
 */
internal inline fun PushConnection.ifInitialized(block: PushConnection.() -> Unit) {
    if (isInitialized()) {
        block()
    } else {
        Logger.error("Native push layer is not yet initialized.")
    }
}
