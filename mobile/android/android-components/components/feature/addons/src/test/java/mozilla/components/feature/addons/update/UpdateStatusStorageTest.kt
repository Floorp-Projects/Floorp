/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.update

import androidx.test.ext.junit.runners.AndroidJUnit4
import junit.framework.TestCase.assertFalse
import junit.framework.TestCase.assertTrue
import mozilla.components.feature.addons.update.DefaultAddonUpdater.UpdateStatusStorage
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class UpdateStatusStorageTest {

    private lateinit var storage: UpdateStatusStorage

    @Before
    fun setup() {
        storage = UpdateStatusStorage()
        storage.clear(testContext)
    }

    @Test
    fun `isPreviouslyAllowed - returns the actual status of an addon`() {
        var allowed = storage.isPreviouslyAllowed(testContext, "addonId")

        assertFalse(allowed)

        storage.markAsAllowed(testContext, "addonId")
        allowed = storage.isPreviouslyAllowed(testContext, "addonId")

        assertTrue(allowed)
    }

    @Test
    fun `markAsUnallowed - deletes only the selected addonId from the storage`() {
        var allowed = storage.isPreviouslyAllowed(testContext, "addonId")

        assertFalse(allowed)

        storage.markAsAllowed(testContext, "addonId")
        storage.markAsAllowed(testContext, "another_addonId")

        allowed = storage.isPreviouslyAllowed(testContext, "addonId")
        assertTrue(allowed)

        allowed = storage.isPreviouslyAllowed(testContext, "another_addonId")
        assertTrue(allowed)

        storage.markAsUnallowed(testContext, "addonId")
        allowed = storage.isPreviouslyAllowed(testContext, "addonId")
        assertFalse(allowed)

        allowed = storage.isPreviouslyAllowed(testContext, "another_addonId")
        assertTrue(allowed)
    }
}
