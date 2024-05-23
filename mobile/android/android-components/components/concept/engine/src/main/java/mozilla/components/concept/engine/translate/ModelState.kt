/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.translate

/**
 * The current state or status of a language model.
 */
enum class ModelState {
    /**
     * The language model(s) are not downloaded to the device.
     */
    NOT_DOWNLOADED,

    /**
     * The device is currently processing downloading the language model(s).
     */
    DOWNLOAD_IN_PROGRESS,

    /**
     * The device is currently processing deleting the language model(s).
     */
    DELETION_IN_PROGRESS,

    /**
     * The language model(s) are downloaded to the device.
     */
    DOWNLOADED,
}
