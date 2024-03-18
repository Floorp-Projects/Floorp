/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.spocs.db

import mozilla.components.service.pocket.helpers.assertClassVisibility
import org.junit.Assert.assertTrue
import org.junit.Test
import kotlin.reflect.KVisibility.INTERNAL

class SpocImpressionEntityTest {
    // This is the data type persisted locally. No need to be public
    @Test
    fun `GIVEN a spoc entity THEN it's visibility is internal`() {
        assertClassVisibility(SpocImpressionEntity::class, INTERNAL)
    }

    @Test
    fun `WHEN a new impression is created THEN the timestamp should be seconds from Epoch`() {
        val nowInSeconds = System.currentTimeMillis() / 1000
        val impression = SpocImpressionEntity(2)

        assertTrue(
            LongRange(nowInSeconds - 5, nowInSeconds + 5)
                .contains(impression.impressionDateInSeconds),
        )
    }
}
