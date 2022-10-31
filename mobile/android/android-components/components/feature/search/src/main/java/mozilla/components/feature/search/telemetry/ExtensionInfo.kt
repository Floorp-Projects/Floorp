/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.telemetry

/**
 * Configuration data of web extensions used for the search / ads telemetry.
 *
 * @property id webExtension unique id.
 * @property resourceUrl location of the webextension (may be local or web hosted).
 * @property messageId message key used for communicating from the extension to the native app.
 */
internal data class ExtensionInfo(
    val id: String,
    val resourceUrl: String,
    val messageId: String,
)
