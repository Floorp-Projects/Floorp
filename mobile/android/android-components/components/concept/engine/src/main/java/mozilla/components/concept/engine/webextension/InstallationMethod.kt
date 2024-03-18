/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.webextension

/**
 *  The method used to install a [WebExtension].
 */
enum class InstallationMethod {
    /**
     * Indicates the [WebExtension] was installed from the add-ons manager.
     */
    MANAGER,

    /**
     * Indicates the [WebExtension] was installed from a file.
     */
    FROM_FILE,
}
