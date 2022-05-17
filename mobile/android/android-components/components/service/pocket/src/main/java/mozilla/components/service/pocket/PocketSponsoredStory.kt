/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

/**
 * A Pocket sponsored story.
 *
 * @property title the title of the story.
 * @property url 3rd party url containing the original story.
 * @property imageUrl a url to a still image representing the story.
 * Contains a "resize" parameter in the form of "resize=w618-h310" allowing to get the image
 * with a specific resolution and the CENTER_CROP ScaleType.
 * @property sponsor 3rd party sponsor of this story, e.g. "NextAdvisor".
 * @property shim Unique identifiers for when the user interacts with this story.
 */
data class PocketSponsoredStory(
    val title: String,
    val url: String,
    val imageUrl: String,
    val sponsor: String,
    val shim: PocketSponsoredStoryShim,
)

/**
 * Sponsored story unique identifiers intended to be used in telemetry.
 *
 * @property click Unique identifier for when the sponsored story is clicked.
 * @property impression Unique identifier for when the user sees this sponsored story.
 */
data class PocketSponsoredStoryShim(
    val click: String,
    val impression: String,
)
