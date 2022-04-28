/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.update

import mozilla.components.service.pocket.helpers.assertClassVisibility
import org.junit.Test
import kotlin.reflect.KVisibility.INTERNAL

class RefreshSpocsWorkerTest {
    @Test
    fun `GIVEN a RefreshSpocsWorker THEN its visibility is internal`() {
        assertClassVisibility(RefreshSpocsWorker::class, INTERNAL)
    }
}
