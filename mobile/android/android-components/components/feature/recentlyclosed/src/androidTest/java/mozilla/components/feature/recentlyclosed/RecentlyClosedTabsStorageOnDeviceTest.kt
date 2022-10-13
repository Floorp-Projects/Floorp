/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.recentlyclosed

import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.Job
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.browser.state.state.recover.TabState
import mozilla.components.concept.base.crash.Breadcrumb
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.concept.engine.EngineSessionStateStorage
import mozilla.components.support.test.fakes.engine.FakeEngine
import mozilla.components.support.test.fakes.engine.FakeEngineSessionState
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class RecentlyClosedTabsStorageOnDeviceTest {
    private val engineState = FakeEngineSessionState("testId")
    private val storage = RecentlyClosedTabsStorage(
        context = ApplicationProvider.getApplicationContext(),
        engine = FakeEngine(),
        crashReporting = FakeCrashReporting(),
        engineStateStorage = FakeEngineSessionStateStorage(),
    )

    @Test
    fun testRowTooBigExceptionCaughtAndStorageCleared() = runBlocking {
        val closedTab1 = RecoverableTab(
            engineSessionState = engineState,
            state = TabState(
                id = "test",
                title = "Pocket",
                url = "test",
                lastAccess = System.currentTimeMillis(),
            ),
        )
        val closedTab2 = closedTab1.copy(
            state = closedTab1.state.copy(
                url = "test".repeat(1_000_000), // much more than 2MB of data. Just to be sure.
            ),
        )

        // First check what happens if too large tabs are persisted and then asked for
        storage.addTabsToCollectionWithMax(listOf(closedTab1, closedTab2), 2)
        assertFalse((storage.engineStateStorage() as FakeEngineSessionStateStorage).data.isEmpty())
        val corruptedTabsResult = storage.getTabs().first()
        assertTrue(corruptedTabsResult.isEmpty())
        assertTrue((storage.engineStateStorage() as FakeEngineSessionStateStorage).data.isEmpty())

        // Then check that new data is persisted and queried successfully
        val closedTab3 = RecoverableTab(
            engineSessionState = engineState,
            state = TabState(
                id = "test2",
                title = "Pocket2",
                url = "test2",
                lastAccess = System.currentTimeMillis(),
            ),
        )
        storage.addTabState(closedTab3)
        val recentlyClosedTabsResult = storage.getTabs().first()
        assertEquals(listOf(closedTab3.state), recentlyClosedTabsResult)
        assertEquals(1, (storage.engineStateStorage() as FakeEngineSessionStateStorage).data.size)
    }
}

private class FakeCrashReporting : CrashReporting {
    override fun submitCaughtException(throwable: Throwable): Job {
        return MainScope().launch {}
    }

    override fun recordCrashBreadcrumb(breadcrumb: Breadcrumb) {}
}

private class FakeEngineSessionStateStorage : EngineSessionStateStorage {
    val data: MutableMap<String, EngineSessionState?> = mutableMapOf()

    override suspend fun write(uuid: String, state: EngineSessionState): Boolean {
        data[uuid] = state
        return true
    }

    override suspend fun read(uuid: String): EngineSessionState? {
        return data[uuid]
    }

    override suspend fun delete(uuid: String) {
        data.remove(uuid)
    }

    override suspend fun deleteAll() {
        data.clear()
    }
}
