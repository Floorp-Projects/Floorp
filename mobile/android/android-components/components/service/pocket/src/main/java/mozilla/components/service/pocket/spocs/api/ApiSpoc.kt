/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.spocs.api

/**
 * A Pocket sponsored as downloaded from the sponsored stories endpoint.
 *
 * @property title the title of the story.
 * @property url 3rd party url containing the original story.
 * @property imageSrc a url to a still image representing the story.
 * Contains a "resize" parameter in the form of "resize=w618-h310" allowing to get the image
 * with a specific resolution and the CENTER_CROP ScaleType.
 * @property sponsor 3rd party sponsor of this story, e.g. "NextAdvisor".
 * @property shim Unique identifiers for when the user interacts with this story.
 */
internal data class ApiSpoc(
    val title: String,
    val url: String,
    val imageSrc: String,
    val sponsor: String,
    val shim: ApiSpocShim,
)

/**
 * Sponsored story unique identifiers intended to be used in telemetry.
 *
 * @property click Unique identifier for when the sponsored story is clicked.
 * @property impression Unique identifier for when the user sees this sponsored story.
 */
internal data class ApiSpocShim(
    val click: String,
    val impression: String,
)
