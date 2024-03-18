/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.storage

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.test.runTest
import mozilla.components.support.test.fakes.engine.FakeEngine
import mozilla.components.support.test.fakes.engine.FakeEngineSessionState
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class FileEngineSessionStateStorageTest {
    @Test
    fun `able to read and write engine session states`() = runTest {
        val storage = FileEngineSessionStateStorage(testContext, FakeEngine())

        // reading non-existing tab
        assertNull(storage.read("test-tab"))

        storage.write("test-tab", FakeEngineSessionState("some-engine-state"))
        val state = storage.read("test-tab")

        assertEquals("some-engine-state", (state as FakeEngineSessionState).value)

        assertNull(storage.read("another-tab"))
    }

    @Test
    fun `able to delete specific engine session states`() = runTest {
        val storage = FileEngineSessionStateStorage(testContext, FakeEngine())
        storage.write("test-tab-1", FakeEngineSessionState("some-engine-state-1"))
        storage.write("test-tab-2", FakeEngineSessionState("some-engine-state-2"))
        storage.write("test-tab-3", FakeEngineSessionState("some-engine-state-3"))
        storage.write("test-tab-4", FakeEngineSessionState("some-engine-state-4"))

        // deleting non-existing tab
        assertNull(storage.read("does-not-exist"))
        storage.delete("does-not-exist")
        assertNull(storage.read("does-not-exist"))

        assertNotNull(storage.read("test-tab-3"))
        storage.delete("test-tab-3")
        assertNull(storage.read("test-tab-3"))

        assertNotNull(storage.read("test-tab-1"))
        storage.delete("test-tab-1")
        assertNull(storage.read("test-tab-1"))

        assertNotNull(storage.read("test-tab-2"))
        storage.delete("test-tab-2")
        assertNull(storage.read("test-tab-2"))

        assertNotNull(storage.read("test-tab-4"))
        storage.delete("test-tab-4")
        assertNull(storage.read("test-tab-4"))
    }

    @Test
    fun `able to delete all engine states`() = runTest {
        val storage = FileEngineSessionStateStorage(testContext, FakeEngine())

        // already empty storage
        storage.deleteAll()

        storage.write("test-tab-1", FakeEngineSessionState("some-engine-state-1"))
        storage.write("test-tab-2", FakeEngineSessionState("some-engine-state-2"))
        storage.write("test-tab-3", FakeEngineSessionState("some-engine-state-3"))
        storage.write("test-tab-4", FakeEngineSessionState("some-engine-state-4"))

        storage.deleteAll()

        assertNull(storage.read("test-tab-1"))
        assertNull(storage.read("test-tab-2"))
        assertNull(storage.read("test-tab-3"))
        assertNull(storage.read("test-tab-4"))
    }
}
