/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.worker

import kotlinx.coroutines.CancellationException
import mozilla.components.concept.engine.webextension.WebExtensionException
import java.io.IOException

/**
 * Indicates if an exception should be reported to the crash reporter.
 */
internal fun Exception.shouldReport(): Boolean {
    val isRecoverable = (this as? WebExtensionException)?.isRecoverable ?: true
    return cause !is IOException &&
        cause !is CancellationException &&
        this !is CancellationException &&
        isRecoverable
}
