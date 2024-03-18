/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.stories.api

import mozilla.components.service.pocket.helpers.assertClassVisibility
import org.junit.Test
import kotlin.reflect.KVisibility

class PocketApiStoryTest {
    // This is the data type as received from the Pocket endpoint. No need to be public.
    @Test
    fun `GIVEN a PocketRecommendedStory THEN its visibility is internal`() {
        assertClassVisibility(PocketApiStory::class, KVisibility.INTERNAL)
    }
}
