package org.mozilla.fenix.debugsettings

import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.debugsettings.store.DebugDrawerAction
import org.mozilla.fenix.debugsettings.store.DebugDrawerState
import org.mozilla.fenix.debugsettings.store.DebugDrawerStore

class DebugDrawerStoreTest {

    @Test
    fun `GIVEN the drawer is closed WHEN the drawer is opened THEN the state should be set to open`() {
        val expected = DebugDrawerState.DrawerStatus.Open
        val store = createStore()

        store.dispatch(DebugDrawerAction.DrawerOpened).joinBlocking()

        assertEquals(expected, store.state.drawerStatus)
    }

    @Test
    fun `GIVEN the drawer is opened WHEN the drawer is closed THEN the state should be set to closed`() {
        val expected = DebugDrawerState.DrawerStatus.Closed
        val store = createStore(
            drawerStatus = DebugDrawerState.DrawerStatus.Open,
        )

        store.dispatch(DebugDrawerAction.DrawerClosed).joinBlocking()

        assertEquals(expected, store.state.drawerStatus)
    }

    private fun createStore(
        drawerStatus: DebugDrawerState.DrawerStatus = DebugDrawerState.DrawerStatus.Closed,
    ) = DebugDrawerStore(
        initialState = DebugDrawerState(
            drawerStatus = drawerStatus,
        ),
    )
}
