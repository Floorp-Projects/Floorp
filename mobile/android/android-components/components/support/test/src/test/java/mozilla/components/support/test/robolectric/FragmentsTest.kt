/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.test.robolectric

import androidx.fragment.app.Fragment
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class FragmentsTest {

    @Test
    fun `setupFragment should add fragment correctly`() {
        val addedFragment = createAddedTestFragment { Fragment() }

        assertTrue(addedFragment.isAdded)
    }

    @Test
    fun `setupFragment should add fragment with correct tag`() {
        val fragment = createAddedTestFragment(fragmentTag = "aTag") { Fragment() }

        assertNotNull(fragment.parentFragmentManager.findFragmentByTag("aTag"))
    }
}
