/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.spocs.api

/**
 * A Pocket sponsored as downloaded from the sponsored stories endpoint.
 *
 * @property flightId Unique id of this story.
 * @property title the title of the story.
 * @property url 3rd party url containing the original story.
 * @property imageSrc a url to a still image representing the story.
 * Contains a "resize" parameter in the form of "resize=w618-h310" allowing to get the image
 * with a specific resolution and the CENTER_CROP ScaleType.
 * @property sponsor 3rd party sponsor of this story, e.g. "NextAdvisor".
 * @property shim Unique identifiers for when the user interacts with this story.
 * @property priority Priority level in deciding which stories to be shown first.
 * A lowest number means a higher priority.
 * @property caps Story caps indented to control the maximum number of times the story should be shown.
 */
internal data class ApiSpoc(
    val flightId: Int,
    val title: String,
    val url: String,
    val imageSrc: String,
    val sponsor: String,
    val shim: ApiSpocShim,
    val priority: Int,
    val caps: ApiSpocCaps,
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

/**
 * Sponsored story caps indented to control the maximum number of times the story should be shown.
 *
 * @property lifetimeCount Lifetime maximum number of times this story should be shown.
 * This is independent from the count based on [flightCount] and [flightPeriod] and must never be reset.
 * @property flightCount Maximum number of times this story should be shown in [flightPeriod].
 * @property flightPeriod Period expressed as a number of seconds in which this story should be shown
 * for at most [flightCount] times.
 * Any time the period comes to an end the [flightCount] count should be restarted.
 * Even if based on [flightCount] and [flightCount] this story can still be shown a couple more times
 * if [lifetimeCount] was met then the story should not be shown anymore.
 */
internal data class ApiSpocCaps(
    val lifetimeCount: Int,
    val flightCount: Int,
    val flightPeriod: Int,
)
