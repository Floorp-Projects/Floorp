/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.GeckoNetworkManager.ManagerState;
import org.mozilla.gecko.GeckoNetworkManager.ManagerEvent;

import static org.junit.Assert.*;

@RunWith(TestRunner.class)
public class GeckoNetworkManagerTest {
    /**
     * Tests the transition matrix.
     */
    @Test
    public void testGetNextState() {
        ManagerState testingState;

        testingState = ManagerState.OffNoListeners;
        assertNull(GeckoNetworkManager.getNextState(testingState, ManagerEvent.disableNotifications));
        assertNull(GeckoNetworkManager.getNextState(testingState, ManagerEvent.stop));
        assertNull(GeckoNetworkManager.getNextState(testingState, ManagerEvent.receivedUpdate));
        assertEquals(ManagerState.OnNoListeners, GeckoNetworkManager.getNextState(testingState, ManagerEvent.start));
        assertEquals(ManagerState.OffWithListeners, GeckoNetworkManager.getNextState(testingState, ManagerEvent.enableNotifications));

        testingState = ManagerState.OnNoListeners;
        assertNull(GeckoNetworkManager.getNextState(testingState, ManagerEvent.start));
        assertNull(GeckoNetworkManager.getNextState(testingState, ManagerEvent.disableNotifications));
        assertEquals(ManagerState.OnWithListeners, GeckoNetworkManager.getNextState(testingState, ManagerEvent.enableNotifications));
        assertEquals(ManagerState.OffNoListeners, GeckoNetworkManager.getNextState(testingState, ManagerEvent.stop));
        assertEquals(ManagerState.OnNoListeners, GeckoNetworkManager.getNextState(testingState, ManagerEvent.receivedUpdate));

        testingState = ManagerState.OnWithListeners;
        assertNull(GeckoNetworkManager.getNextState(testingState, ManagerEvent.start));
        assertNull(GeckoNetworkManager.getNextState(testingState, ManagerEvent.enableNotifications));
        assertEquals(ManagerState.OffWithListeners, GeckoNetworkManager.getNextState(testingState, ManagerEvent.stop));
        assertEquals(ManagerState.OnNoListeners, GeckoNetworkManager.getNextState(testingState, ManagerEvent.disableNotifications));
        assertEquals(ManagerState.OnWithListeners, GeckoNetworkManager.getNextState(testingState, ManagerEvent.receivedUpdate));

        testingState = ManagerState.OffWithListeners;
        assertNull(GeckoNetworkManager.getNextState(testingState, ManagerEvent.stop));
        assertNull(GeckoNetworkManager.getNextState(testingState, ManagerEvent.enableNotifications));
        assertNull(GeckoNetworkManager.getNextState(testingState, ManagerEvent.receivedUpdate));
        assertEquals(ManagerState.OnWithListeners, GeckoNetworkManager.getNextState(testingState, ManagerEvent.start));
        assertEquals(ManagerState.OffNoListeners, GeckoNetworkManager.getNextState(testingState, ManagerEvent.disableNotifications));
    }
}