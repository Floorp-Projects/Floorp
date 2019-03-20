/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

/**
 * Exception while downloading experiments from the server
 */
internal class ExperimentDownloadException : Exception {
    constructor(message: String?) : super(message)
    constructor(cause: Throwable) : super(cause)
}
