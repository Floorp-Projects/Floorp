/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.prompt

/**
 * Represents data to share for the Web Share and Web Share Target APIs.
 * https://w3c.github.io/web-share/
 * @property title Title for the share request.
 * @property text Text for the share request.
 * @property url URL for the share request.
 */
data class ShareData(
    val title: String?,
    val text: String?,
    val url: String?
)
