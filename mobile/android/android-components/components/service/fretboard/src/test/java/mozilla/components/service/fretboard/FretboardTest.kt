/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard

import android.content.Context
import android.content.SharedPreferences
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import kotlinx.coroutines.experimental.CommonPool
import kotlinx.coroutines.experimental.runBlocking
import mozilla.components.support.test.any
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doAnswer
import org.mockito.Mockito.mock
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class FretboardTest {
    @Test
    fun testLoadExperiments() {
        val experimentSource = mock(ExperimentSource::class.java)
        val experimentStorage = mock(ExperimentStorage::class.java)
        val fretboard = Fretboard(experimentSource, experimentStorage)
        fretboard.loadExperiments()
        verify(experimentStorage).retrieve()
    }

    @Test
    fun testUpdateExperimentsStorageNotLoaded() {
        val experimentSource = mock(ExperimentSource::class.java)
        val experimentStorage = mock(ExperimentStorage::class.java)
        val fretboard = Fretboard(experimentSource, experimentStorage)
        fretboard.updateExperiments()
        verify(experimentStorage, times(1)).retrieve()
        fretboard.updateExperiments()
        verify(experimentStorage, times(1)).retrieve()
    }

    @Test
    fun testUpdateExperimentsEmptyStorage() {
        val experimentSource = mock(ExperimentSource::class.java)
        val result = ExperimentsSnapshot(listOf(), null)
        `when`(experimentSource.getExperiments(result)).thenReturn(ExperimentsSnapshot(listOf(Experiment("id", "name")), null))
        val experimentStorage = mock(ExperimentStorage::class.java)
        `when`(experimentStorage.retrieve()).thenReturn(result)
        val fretboard = Fretboard(experimentSource, experimentStorage)
        fretboard.updateExperiments()
        verify(experimentSource).getExperiments(result)
        verify(experimentStorage).save(ExperimentsSnapshot(listOf(Experiment("id", "name")), null))
    }

    @Test
    fun testUpdateExperimentsFromStorage() {
        val experimentSource = mock(ExperimentSource::class.java)
        `when`(experimentSource.getExperiments(ExperimentsSnapshot(listOf(Experiment("id0", "name0")), null))).thenReturn(ExperimentsSnapshot(listOf(Experiment("id", "name")), null))
        val experimentStorage = mock(ExperimentStorage::class.java)
        `when`(experimentStorage.retrieve()).thenReturn(ExperimentsSnapshot(listOf(Experiment("id0", "name0")), null))
        val fretboard = Fretboard(experimentSource, experimentStorage)
        fretboard.updateExperiments()
        verify(experimentSource).getExperiments(ExperimentsSnapshot(listOf(Experiment("id0", "name0")), null))
        verify(experimentStorage).save(ExperimentsSnapshot(listOf(Experiment("id", "name")), null))
    }

    @Test
    fun testExperiments() {
        val experimentSource = mock(ExperimentSource::class.java)
        val experimentStorage = mock(ExperimentStorage::class.java)
        val experiments = listOf(
            Experiment("first-id", "first-name"),
            Experiment("second-id", "second-name")
        )
        `when`(experimentStorage.retrieve()).thenReturn(ExperimentsSnapshot(experiments, null))
        val fretboard = Fretboard(experimentSource, experimentStorage)
        var returnedExperiments = fretboard.experiments
        assertEquals(0, returnedExperiments.size)
        fretboard.loadExperiments()
        returnedExperiments = fretboard.experiments
        assertEquals(2, returnedExperiments.size)
        assertTrue(returnedExperiments.contains(experiments[0]))
        assertTrue(returnedExperiments.contains(experiments[1]))
    }

    @Test
    fun testExperimentsNoExperiments() {
        val experimentSource = mock(ExperimentSource::class.java)
        val experimentStorage = mock(ExperimentStorage::class.java)
        val experiments = listOf<Experiment>()
        `when`(experimentStorage.retrieve()).thenReturn(ExperimentsSnapshot(experiments, null))
        val fretboard = Fretboard(experimentSource, experimentStorage)
        val returnedExperiments = fretboard.experiments
        assertEquals(0, returnedExperiments.size)
    }

    @Test
    fun testGetActiveExperiments() {
        val experimentSource = mock(ExperimentSource::class.java)
        val experimentStorage = mock(ExperimentStorage::class.java)
        val experiments = listOf(
            Experiment("first-id",
                name = "first-name",
                match = Experiment.Matcher(
                    manufacturer = "manufacturer-1"
                )
            ),
            Experiment("second-id",
                name = "second-name",
                match = Experiment.Matcher(
                    manufacturer = "unknown",
                    appId = "test.appId"
                )
            ),
            Experiment("third-id",
                name = "third-name",
                match = Experiment.Matcher(
                    manufacturer = "unknown",
                    version = "version.name"
                )
            )
        )
        `when`(experimentStorage.retrieve()).thenReturn(ExperimentsSnapshot(experiments, null))
        val fretboard = Fretboard(experimentSource, experimentStorage)
        fretboard.loadExperiments()

        val context = mock(Context::class.java)
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPrefs = mock(SharedPreferences::class.java)
        val prefsEditor = mock(SharedPreferences.Editor::class.java)
        `when`(prefsEditor.putString(ArgumentMatchers.anyString(), ArgumentMatchers.anyString())).thenReturn(prefsEditor)
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(sharedPrefs.getBoolean(ArgumentMatchers.anyString(), ArgumentMatchers.anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(sharedPrefs)

        val packageInfo = mock(PackageInfo::class.java)
        packageInfo.versionName = "version.name"
        val packageManager = mock(PackageManager::class.java)
        `when`(packageManager.getPackageInfo(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val activeExperiments = fretboard.getActiveExperiments(context)
        assertEquals(2, activeExperiments.size)
        assertTrue(activeExperiments.any { it.id == "second-id" })
        assertTrue(activeExperiments.any { it.id == "third-id" })
    }

    @Test
    fun testGetExperimentsMap() {
        val experimentSource = mock(ExperimentSource::class.java)
        val experimentStorage = mock(ExperimentStorage::class.java)
        val experiments = listOf(
                Experiment("first-id",
                        name = "first-name",
                        match = Experiment.Matcher(
                                manufacturer = "manufacturer-1"
                        )
                ),
                Experiment("second-id",
                        name = "second-name",
                        match = Experiment.Matcher(
                                manufacturer = "unknown",
                                appId = "test.appId"
                        )
                ),
                Experiment("third-id",
                        name = "third-name",
                        match = Experiment.Matcher(
                                manufacturer = "unknown",
                                version = "version.name"
                        )
                )
        )
        `when`(experimentStorage.retrieve()).thenReturn(ExperimentsSnapshot(experiments, null))
        val fretboard = Fretboard(experimentSource, experimentStorage)
        fretboard.loadExperiments()

        val context = mock(Context::class.java)
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPrefs = mock(SharedPreferences::class.java)
        val prefsEditor = mock(SharedPreferences.Editor::class.java)
        `when`(prefsEditor.putString(ArgumentMatchers.anyString(), ArgumentMatchers.anyString())).thenReturn(prefsEditor)
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(sharedPrefs.getBoolean(ArgumentMatchers.anyString(), ArgumentMatchers.anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(sharedPrefs)

        val packageInfo = mock(PackageInfo::class.java)
        packageInfo.versionName = "version.name"
        val packageManager = mock(PackageManager::class.java)
        `when`(packageManager.getPackageInfo(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val experimentsMap = fretboard.getExperimentsMap(context)
        assertEquals(3, experimentsMap.size)
        println(experimentsMap.toString())
        assertTrue(experimentsMap["first-name"] == false)
        assertTrue(experimentsMap["second-name"] == true)
        assertTrue(experimentsMap["third-name"] == true)
    }

    @Test
    fun testIsInExperiment() {
        val experimentSource = mock(ExperimentSource::class.java)
        val experimentStorage = mock(ExperimentStorage::class.java)
        var experiments = listOf(
            Experiment("first-id",
                name = "first-name",
                match = Experiment.Matcher(
                    appId = "test.appId"
                )
            )
        )
        `when`(experimentStorage.retrieve()).thenReturn(ExperimentsSnapshot(experiments, null))
        var fretboard = Fretboard(experimentSource, experimentStorage)
        fretboard.loadExperiments()

        val context = mock(Context::class.java)
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPrefs = mock(SharedPreferences::class.java)
        val prefsEditor = mock(SharedPreferences.Editor::class.java)
        `when`(prefsEditor.putString(ArgumentMatchers.anyString(), ArgumentMatchers.anyString())).thenReturn(prefsEditor)
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(sharedPrefs.getBoolean(ArgumentMatchers.anyString(), ArgumentMatchers.anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(sharedPrefs)

        val packageInfo = mock(PackageInfo::class.java)
        packageInfo.versionName = "version.name"
        val packageManager = mock(PackageManager::class.java)
        `when`(packageManager.getPackageInfo(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        assertTrue(fretboard.isInExperiment(context, ExperimentDescriptor("first-name")))

        experiments = listOf(
            Experiment("first-id",
                name = "first-name",
                match = Experiment.Matcher(
                    appId = "other.appId"
                )
            )
        )
        `when`(experimentStorage.retrieve()).thenReturn(ExperimentsSnapshot(experiments, null))
        fretboard = Fretboard(experimentSource, experimentStorage)

        assertFalse(fretboard.isInExperiment(context, ExperimentDescriptor("first-name")))
        assertFalse(fretboard.isInExperiment(context, ExperimentDescriptor("other-name")))
    }

    @Test
    fun testWithExperiment() {
        val experimentSource = mock(ExperimentSource::class.java)
        val experimentStorage = mock(ExperimentStorage::class.java)
        var experiments = listOf(
            Experiment("first-id",
                name = "first-name",
                match = Experiment.Matcher(
                    appId = "test.appId"
                )
            )
        )
        `when`(experimentStorage.retrieve()).thenReturn(ExperimentsSnapshot(experiments, null))
        var fretboard = Fretboard(experimentSource, experimentStorage)
        fretboard.loadExperiments()

        val context = mock(Context::class.java)
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPrefs = mock(SharedPreferences::class.java)
        val prefsEditor = mock(SharedPreferences.Editor::class.java)
        `when`(prefsEditor.putString(ArgumentMatchers.anyString(), ArgumentMatchers.anyString())).thenReturn(prefsEditor)
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(sharedPrefs.getBoolean(ArgumentMatchers.anyString(), ArgumentMatchers.anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(sharedPrefs)

        val packageInfo = mock(PackageInfo::class.java)
        packageInfo.versionName = "version.name"
        val packageManager = mock(PackageManager::class.java)
        `when`(packageManager.getPackageInfo(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        var invocations = 0
        fretboard.withExperiment(context, ExperimentDescriptor("first-name")) {
            invocations++
            assertEquals(experiments[0], it)
        }
        assertEquals(1, invocations)

        experiments = listOf(
            Experiment("first-id",
                name = "first-name",
                match = Experiment.Matcher(
                    appId = "other.appId"
                )
            )
        )
        `when`(experimentStorage.retrieve()).thenReturn(ExperimentsSnapshot(experiments, null))
        fretboard = Fretboard(experimentSource, experimentStorage)

        invocations = 0
        fretboard.withExperiment(context, ExperimentDescriptor("first-name")) {
            invocations++
        }
        assertEquals(0, invocations)

        invocations = 0
        fretboard.withExperiment(context, ExperimentDescriptor("other-name")) {
            invocations++
        }
        assertEquals(0, invocations)
    }

    @Test
    fun testGetExperiment() {
        val experimentSource = mock(ExperimentSource::class.java)
        val experimentStorage = mock(ExperimentStorage::class.java)
        val experiments = listOf(
            Experiment("first-id",
                name = "first-name",
                match = Experiment.Matcher(
                    appId = "test.appId"
                )
            )
        )
        `when`(experimentStorage.retrieve()).thenReturn(ExperimentsSnapshot(experiments, null))
        val fretboard = Fretboard(experimentSource, experimentStorage)
        fretboard.loadExperiments()

        assertEquals(experiments[0], fretboard.getExperiment(ExperimentDescriptor("first-name")))
        assertNull(fretboard.getExperiment(ExperimentDescriptor("other-name")))
    }

    @Test
    fun testSetOverride() {
        val experimentSource = mock(ExperimentSource::class.java)
        val experimentStorage = mock(ExperimentStorage::class.java)
        val experiments = listOf(
            Experiment("first-id",
                name = "first-name",
                match = Experiment.Matcher(
                    appId = "test.appId"
                )
            )
        )
        `when`(experimentStorage.retrieve()).thenReturn(ExperimentsSnapshot(experiments, null))
        val fretboard = Fretboard(experimentSource, experimentStorage)
        fretboard.loadExperiments()

        val context = mock(Context::class.java)
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPrefs = mock(SharedPreferences::class.java)
        val prefsEditor = mock(SharedPreferences.Editor::class.java)
        `when`(prefsEditor.putBoolean(ArgumentMatchers.anyString(), ArgumentMatchers.anyBoolean())).thenReturn(prefsEditor)
        `when`(prefsEditor.putString(ArgumentMatchers.anyString(), ArgumentMatchers.anyString())).thenReturn(prefsEditor)
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(sharedPrefs.getBoolean(ArgumentMatchers.anyString(), ArgumentMatchers.anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(sharedPrefs)

        val packageInfo = mock(PackageInfo::class.java)
        packageInfo.versionName = "version.name"
        val packageManager = mock(PackageManager::class.java)
        `when`(packageManager.getPackageInfo(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        assertTrue(fretboard.isInExperiment(context, ExperimentDescriptor("first-name")))
        fretboard.setOverride(context, ExperimentDescriptor("first-name"), false)
        verify(prefsEditor).putBoolean("first-name", false)
        fretboard.setOverride(context, ExperimentDescriptor("first-name"), true)
        verify(prefsEditor).putBoolean("first-name", true)

        runBlocking(CommonPool) {
            assertTrue(fretboard.isInExperiment(context, ExperimentDescriptor("first-name")))
            fretboard.setOverrideNow(context, ExperimentDescriptor("first-name"), false)
            verify(prefsEditor, times(2)).putBoolean("first-name", false)
            fretboard.setOverrideNow(context, ExperimentDescriptor("first-name"), true)
            verify(prefsEditor, times(2)).putBoolean("first-name", true)
        }
    }

    @Test
    fun testClearOverride() {
        val experimentSource = mock(ExperimentSource::class.java)
        val experimentStorage = mock(ExperimentStorage::class.java)
        val experiments = listOf(
            Experiment("first-id",
                name = "first-name",
                match = Experiment.Matcher(
                    appId = "test.appId"
                )
            )
        )
        `when`(experimentStorage.retrieve()).thenReturn(ExperimentsSnapshot(experiments, null))
        val fretboard = Fretboard(experimentSource, experimentStorage)
        fretboard.loadExperiments()

        val context = mock(Context::class.java)
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPrefs = mock(SharedPreferences::class.java)
        val prefsEditor = mock(SharedPreferences.Editor::class.java)
        `when`(prefsEditor.remove(ArgumentMatchers.anyString())).thenReturn(prefsEditor)
        `when`(prefsEditor.putBoolean(ArgumentMatchers.anyString(), ArgumentMatchers.anyBoolean())).thenReturn(prefsEditor)
        `when`(prefsEditor.putString(ArgumentMatchers.anyString(), ArgumentMatchers.anyString())).thenReturn(prefsEditor)
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(sharedPrefs.getBoolean(ArgumentMatchers.anyString(), ArgumentMatchers.anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(sharedPrefs)

        val packageInfo = mock(PackageInfo::class.java)
        packageInfo.versionName = "version.name"
        val packageManager = mock(PackageManager::class.java)
        `when`(packageManager.getPackageInfo(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        assertTrue(fretboard.isInExperiment(context, ExperimentDescriptor("first-name")))
        fretboard.setOverride(context, ExperimentDescriptor("first-name"), false)
        fretboard.clearOverride(context, ExperimentDescriptor("first-name"))
        verify(prefsEditor).remove("first-name")

        runBlocking(CommonPool) {
            fretboard.setOverrideNow(context, ExperimentDescriptor("first-name"), false)
            fretboard.clearOverrideNow(context, ExperimentDescriptor("first-name"))
            verify(prefsEditor, times(2)).remove("first-name")
        }
    }

    @Test
    fun testClearAllOverrides() {
        val experimentSource = mock(ExperimentSource::class.java)
        val experimentStorage = mock(ExperimentStorage::class.java)
        val experiments = listOf(
            Experiment("first-id",
                name = "first-name",
                match = Experiment.Matcher(
                    appId = "test.appId"
                )
            )
        )
        `when`(experimentStorage.retrieve()).thenReturn(ExperimentsSnapshot(experiments, null))
        val fretboard = Fretboard(experimentSource, experimentStorage)
        fretboard.loadExperiments()

        val context = mock(Context::class.java)
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPrefs = mock(SharedPreferences::class.java)
        val prefsEditor = mock(SharedPreferences.Editor::class.java)
        `when`(prefsEditor.clear()).thenReturn(prefsEditor)
        `when`(prefsEditor.putBoolean(ArgumentMatchers.anyString(), ArgumentMatchers.anyBoolean())).thenReturn(prefsEditor)
        `when`(prefsEditor.putString(ArgumentMatchers.anyString(), ArgumentMatchers.anyString())).thenReturn(prefsEditor)
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(sharedPrefs.getBoolean(ArgumentMatchers.anyString(), ArgumentMatchers.anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(sharedPrefs)

        val packageInfo = mock(PackageInfo::class.java)
        packageInfo.versionName = "version.name"
        val packageManager = mock(PackageManager::class.java)
        `when`(packageManager.getPackageInfo(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        assertTrue(fretboard.isInExperiment(context, ExperimentDescriptor("first-name")))
        fretboard.setOverride(context, ExperimentDescriptor("first-name"), false)
        fretboard.clearAllOverrides(context)
        verify(prefsEditor).clear()

        runBlocking(CommonPool) {
            fretboard.setOverrideNow(context, ExperimentDescriptor("first-name"), false)
            fretboard.clearAllOverridesNow(context)
            verify(prefsEditor, times(2)).clear()
        }
    }

    @Test
    fun testUpdateExperimentsException() {
        val source = mock(ExperimentSource::class.java)
        doAnswer {
            throw ExperimentDownloadException("test")
        }.`when`(source).getExperiments(any())
        val storage = mock(ExperimentStorage::class.java)
        `when`(storage.retrieve()).thenReturn(ExperimentsSnapshot(listOf(), null))
        val fretboard = Fretboard(source, storage)
        fretboard.updateExperiments()
    }
}