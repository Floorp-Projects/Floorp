/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.shadows.ShadowLooper

@RunWith(AndroidJUnit4::class)
class StoreExceptionTest {
    // This test is in a separate class because it needs to run with Robolectric (different runner, slower) while all
    // other tests only need a Java VM (fast).
    @Test(expected = StoreException::class)
    fun `Exception in reducer will be rethrown on main thread`() {
        val throwingReducer: (TestState, TestAction) -> TestState = { _, _ ->
            throw IllegalStateException("Not reducing today")
        }

        val store = Store(TestState(counter = 23), throwingReducer)

        store.dispatch(TestAction.IncrementAction).joinBlocking()

        // Wait for the main looper to process the re-thrown exception.
        ShadowLooper.idleMainLooper()

        Assert.fail()
    }
}
