/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.service.pocket.helpers.assertClassVisibility
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.reflect.KVisibility

@RunWith(AndroidJUnit4::class)
class PocketRecommendedStoryTest {

    // This is the domain data type we expose to clients. Needs to be public.
    @Test
    fun `GIVEN a PocketRecommendedStory THEN its visibility is public`() {
        assertClassVisibility(PocketRecommendedStory::class, KVisibility.PUBLIC)
    }
}
