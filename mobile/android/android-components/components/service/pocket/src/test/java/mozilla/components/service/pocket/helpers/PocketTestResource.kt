/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.helpers

private const val POCKET_DIR = "pocket"

/**
 * Accessors to resources used in testing. These files are available in `app/src/test/resources`.
 */
internal enum class PocketTestResource(private val path: String) {
    FIVE_POCKET_STORIES_RECOMMENDATIONS_POCKET_RESPONSE("$POCKET_DIR/stories_recommendations_response.json"),
    ONE_POCKET_STORY_RECOMMENDATION_POCKET_RESPONSE("$POCKET_DIR/story_recommendation_response.json"),
    POCKET_STORY_JSON("$POCKET_DIR/story.json");

    /** @return the raw resource. */
    fun get(): String = this::class.java.classLoader!!.getResource(path)!!.readText()
}
