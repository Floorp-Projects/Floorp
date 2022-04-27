/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.update

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.service.pocket.helpers.assertClassVisibility
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.reflect.KVisibility

@RunWith(AndroidJUnit4::class)
class RefreshPocketWorkerTest {

    @Test
    fun `GIVEN a RefreshPocketWorker THEN its visibility is internal`() {
        assertClassVisibility(RefreshPocketWorker::class, KVisibility.INTERNAL)
    }
}
