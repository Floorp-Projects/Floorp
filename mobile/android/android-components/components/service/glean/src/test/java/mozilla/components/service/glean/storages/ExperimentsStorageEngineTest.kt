/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package mozilla.components.service.glean.storages

import java.util.ArrayList

import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.Assert.assertEquals
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class ExperimentsStorageEngineTest {
    @Before
    fun setUp() {
        ExperimentsStorageEngine.clearAllStores()
    }

    @After
    fun clear() {
        ExperimentsStorageEngine.clearAllStores()
    }

    @Test
    fun `setExperimentActive() properly sets the value in all stores`() {
        ExperimentsStorageEngine.setExperimentActive(
            experimentId = "experiment_a",
            branch = "branch_a",
            extra = mapOf("extra_key_1" to "extra_value_1")
        )

        val snapshot = ExperimentsStorageEngine.getSnapshot()
        assertEquals(1, snapshot.size)
        assertEquals("branch_a", snapshot.get("experiment_a")!!.branch)
        assertEquals(
            "extra_value_1",
            snapshot.get("experiment_a")!!.extra!!.get("extra_key_1")
        )
    }

    @Test
    fun `setExperimentActive() properly truncates experiment and branch names`() {
        ExperimentsStorageEngine.setExperimentActive(
            experimentId = "e0123456789012345678901234567890123456789",
            branch = "b0123456789012345678901234567890123456789"
        )

        val snapshot = ExperimentsStorageEngine.getSnapshot()
        assertEquals(1, snapshot.size)
        assertEquals(
            "b01234567890123456789012345678",
            snapshot.get("e01234567890123456789012345678")!!.branch
        )

        // Removing a long name should still work, despite truncation
        ExperimentsStorageEngine.setExperimentInactive(
            experimentId = "e0123456789012345678901234567890123456789"
        )

        val snapshot2 = ExperimentsStorageEngine.getSnapshot()
        assertEquals(0, snapshot2.size)
    }

    @Test
    fun `test behavior of adding and removing experiments`() {
        ExperimentsStorageEngine.setExperimentActive(
            experimentId = "experiment_1",
            branch = "branch_a"
        )
        ExperimentsStorageEngine.setExperimentActive(
            experimentId = "experiment_2",
            branch = "branch_c"
        )

        val snapshot = ExperimentsStorageEngine.getSnapshot()
        assertEquals(2, snapshot.size)
        assertEquals(
            listOf("experiment_1", "experiment_2"),
            ArrayList(snapshot.keys)
        )

        ExperimentsStorageEngine.setExperimentActive(
            experimentId = "experiment_1",
            branch = "branch_b"
        )
        val snapshot2 = ExperimentsStorageEngine.getSnapshot()
        assertEquals(2, snapshot2.size)
        assertEquals("branch_b", snapshot2.get("experiment_1")!!.branch)

        ExperimentsStorageEngine.setExperimentInactive("experiment_2")
        val snapshot3 = ExperimentsStorageEngine.getSnapshot()
        assertEquals(1, snapshot3.size)
        assertEquals("branch_b", snapshot3.get("experiment_1")!!.branch)

        // Remove non-existent experiment
        ExperimentsStorageEngine.setExperimentInactive("ridiculous")
        val snapshot4 = ExperimentsStorageEngine.getSnapshot()
        assertEquals(1, snapshot4.size)
        assertEquals("branch_b", snapshot4.get("experiment_1")!!.branch)
    }

    @Test
    fun `test JSON output`() {
        ExperimentsStorageEngine.setExperimentActive(
            experimentId = "experiment_1",
            branch = "branch_a"
        )
        ExperimentsStorageEngine.setExperimentActive(
            experimentId = "experiment_2",
            branch = "branch_c",
            extra = mapOf("extra_key_1" to "extra_val_1")
        )

        val json = ExperimentsStorageEngine.getSnapshotAsJSON(
            "test", true
        )
        assertEquals(
            "{\"experiment_1\":{\"branch\":\"branch_a\"},\"experiment_2\":{\"branch\":\"branch_c\",\"extra\":{\"extra_key_1\":\"extra_val_1\"}}}",
            json.toString()
        )
    }
}
