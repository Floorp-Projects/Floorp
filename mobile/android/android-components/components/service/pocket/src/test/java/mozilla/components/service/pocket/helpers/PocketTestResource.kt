/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.helpers

private const val POCKET_DIR = "pocket"

/**
 * Accessors to resources used in testing. These files are available in `app/src/test/resources`.
 */
enum class PocketTestResource(private val path: String) {
    POCKET_VIDEO_RECOMMENDATION("$POCKET_DIR/video_recommendations.json");

    fun get(): String = this::class.java.classLoader!!.getResource(path)!!.readText()
}
