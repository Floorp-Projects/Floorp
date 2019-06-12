/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

import android.content.Context
import android.content.SharedPreferences
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyString
import org.mockito.ArgumentMatchers.eq
import org.mockito.Mockito.`when`
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class ActiveExperimentTest {
    private lateinit var context: Context
    private lateinit var sharedPrefs: SharedPreferences
    private lateinit var prefsEditor: SharedPreferences.Editor
    private val prefNameActiveExperiment = ActiveExperiment.PREF_NAME_ACTIVE_EXPERIMENT
    private val prefKeyId = ActiveExperiment.KEY_ID
    private val prefKeyBranch = ActiveExperiment.KEY_BRANCH

    private fun testReset() {
        context = mock()
        sharedPrefs = mock()
        prefsEditor = mock()

        `when`(context.getSharedPreferences(eq(prefNameActiveExperiment), eq(Context.MODE_PRIVATE))).thenReturn(sharedPrefs)
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(prefsEditor.putString(anyString(), anyString())).thenReturn(prefsEditor)
        `when`(prefsEditor.clear()).thenReturn(prefsEditor)
    }

    private fun setActiveExperimentPrefs(id: String, branch: String) {
        `when`(sharedPrefs.getString(eq(prefKeyId), eq(null))).thenReturn(id)
        `when`(sharedPrefs.getString(eq(prefKeyBranch), eq(null))).thenReturn(branch)
    }

    @Test
    fun `test basics`() {
        val experiment = createDefaultExperiment(
            id = "id",
            branches = listOf(
                Experiment.Branch("branch1", 1),
                Experiment.Branch("branch2", 2)
            )
        )

        val activeExperiment = ActiveExperiment(experiment, "branch2")
        assertEquals(activeExperiment.experiment, experiment)
        assertEquals(activeExperiment.branch, "branch2")
    }

    @Test
    fun `test load from storage`() {
        testReset()
        val experiments = listOf(
            createDefaultExperiment(
                id = "id1",
                branches = listOf(
                    Experiment.Branch("branch1", 2),
                    Experiment.Branch("branch2", 3)
                )
            ),
            createDefaultExperiment(
                id = "id2",
                branches = listOf(
                    Experiment.Branch("branch3", 2)
                )
            )
        )

        // Start with empty storage.
        assertNull(ActiveExperiment.load(context, experiments))
        verify(context).getSharedPreferences(prefNameActiveExperiment, Context.MODE_PRIVATE)
        verify(sharedPrefs).getString(prefKeyId, null)

        // Test valid experiment id and branch name variations.
        setActiveExperimentPrefs("id1", "branch1")
        var active = ActiveExperiment.load(context, experiments)
        assertEquals(experiments[0], active!!.experiment)
        assertEquals("branch1", active.branch)

        setActiveExperimentPrefs("id1", "branch2")
        active = ActiveExperiment.load(context, experiments)
        assertEquals(experiments[0], active!!.experiment)
        assertEquals("branch2", active.branch)

        setActiveExperimentPrefs("id2", "branch3")
        active = ActiveExperiment.load(context, experiments)
        assertEquals(experiments[1], active!!.experiment)
        assertEquals("branch3", active.branch)

        // Test invalid experiment id and branch name variations.
        testReset()
        setActiveExperimentPrefs("id2", "non-existent")
        assertNull(ActiveExperiment.load(context, experiments))
        verify(context).getSharedPreferences(prefNameActiveExperiment, Context.MODE_PRIVATE)
        verify(sharedPrefs).getString(prefKeyId, null)
        verify(sharedPrefs).getString(prefKeyBranch, null)

        setActiveExperimentPrefs("non-existent", "branch1")
        assertNull(ActiveExperiment.load(context, experiments))
        verify(context, times(2)).getSharedPreferences(prefNameActiveExperiment, Context.MODE_PRIVATE)
        verify(sharedPrefs, times(2)).getString(prefKeyId, null)
        verify(sharedPrefs, times(2)).getString(prefKeyBranch, null)
    }

    @Test
    fun `test saving state`() {
        testReset()

        val experiment = createDefaultExperiment(
            id = "some-id",
            branches = listOf(Experiment.Branch("some-branch", 3))
        )
        val active = ActiveExperiment(experiment, "some-branch")

        active.save(context)
        verify(context).getSharedPreferences(prefNameActiveExperiment, Context.MODE_PRIVATE)
        verify(sharedPrefs).edit()
        verify(prefsEditor).putString(prefKeyId, "some-id")
        verify(prefsEditor).putString(prefKeyBranch, "some-branch")
        verify(prefsEditor).apply()
    }

    @Test
    fun `test clearing state`() {
        testReset()

        ActiveExperiment.clear(context)
        verify(context).getSharedPreferences(prefNameActiveExperiment, Context.MODE_PRIVATE)
        verify(sharedPrefs).edit()
        verify(prefsEditor).clear()
        verify(prefsEditor).apply()
    }
}
