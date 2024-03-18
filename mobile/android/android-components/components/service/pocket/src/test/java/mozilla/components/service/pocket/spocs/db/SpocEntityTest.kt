/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.spocs.db

import mozilla.components.service.pocket.helpers.assertClassVisibility
import org.junit.Test
import kotlin.reflect.KVisibility.INTERNAL

class SpocEntityTest {
    // This is the data type persisted locally. No need to be public
    @Test
    fun `GIVEN a spoc entity THEN it's visibility is internal`() {
        assertClassVisibility(SpocEntity::class, INTERNAL)
    }
}
