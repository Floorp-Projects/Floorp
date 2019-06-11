/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard

import android.content.Context
import android.content.SharedPreferences
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.runBlocking
import mozilla.components.service.fretboard.storage.flatfile.FlatFileExperimentStorage
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
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
import java.io.File
import kotlin.reflect.full.functions
import kotlin.reflect.jvm.isAccessible

@RunWith(AndroidJUnit4::class)
class FretboardTest {

    @Test
    fun loadExperiments() {
        val experimentSource = mock<ExperimentSource>()
        val experimentStorage = mock<ExperimentStorage>()
        val fretboard = Fretboard(experimentSource, experimentStorage)
        fretboard.loadExperiments()
        verify(experimentStorage).retrieve()
    }

    @Test
    fun updateExperimentsStorageNotLoaded() {
        val experimentSource = mock<ExperimentSource>()
        val experimentStorage = mock<ExperimentStorage>()
        val fretboard = Fretboard(experimentSource, experimentStorage)
        fretboard.updateExperiments()
        verify(experimentStorage, times(1)).retrieve()
        fretboard.updateExperiments()
        verify(experimentStorage, times(1)).retrieve()
    }

    @Test
    fun updateExperimentsEmptyStorage() {
        val experimentSource = mock<ExperimentSource>()
        val result = ExperimentsSnapshot(listOf(), null)
        `when`(experimentSource.getExperiments(result)).thenReturn(ExperimentsSnapshot(listOf(Experiment("id", "name")), null))
        val experimentStorage = mock<ExperimentStorage>()
        `when`(experimentStorage.retrieve()).thenReturn(result)
        val fretboard = Fretboard(experimentSource, experimentStorage)
        fretboard.updateExperiments()
        verify(experimentSource).getExperiments(result)
        verify(experimentStorage).save(ExperimentsSnapshot(listOf(Experiment("id", "name")), null))
    }

    @Test
    fun updateExperimentsFromStorage() {
        val experimentSource = mock<ExperimentSource>()
        `when`(experimentSource.getExperiments(ExperimentsSnapshot(listOf(Experiment("id0", "name0")), null))).thenReturn(ExperimentsSnapshot(listOf(Experiment("id", "name")), null))
        val experimentStorage = mock<ExperimentStorage>()
        `when`(experimentStorage.retrieve()).thenReturn(ExperimentsSnapshot(listOf(Experiment("id0", "name0")), null))
        val fretboard = Fretboard(experimentSource, experimentStorage)
        fretboard.updateExperiments()
        verify(experimentSource).getExperiments(ExperimentsSnapshot(listOf(Experiment("id0", "name0")), null))
        verify(experimentStorage).save(ExperimentsSnapshot(listOf(Experiment("id", "name")), null))
    }

    @Test
    fun experiments() {
        val experimentSource = mock<ExperimentSource>()
        val experimentStorage = mock<ExperimentStorage>()
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
    fun experimentsNoExperiments() {
        val experimentSource = mock<ExperimentSource>()
        val experimentStorage = mock<ExperimentStorage>()
        val experiments = listOf<Experiment>()
        `when`(experimentStorage.retrieve()).thenReturn(ExperimentsSnapshot(experiments, null))
        val fretboard = Fretboard(experimentSource, experimentStorage)
        val returnedExperiments = fretboard.experiments
        assertEquals(0, returnedExperiments.size)
    }

    @Test
    fun getActiveExperiments() {
        val experimentSource = mock<ExperimentSource>()
        val experimentStorage = mock<ExperimentStorage>()
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

        val context = mock<Context>()
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPrefs = mock<SharedPreferences>()
        val prefsEditor = mock(SharedPreferences.Editor::class.java)
        `when`(prefsEditor.putString(ArgumentMatchers.anyString(), ArgumentMatchers.anyString())).thenReturn(prefsEditor)
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(sharedPrefs.getBoolean(ArgumentMatchers.anyString(), ArgumentMatchers.anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(sharedPrefs)

        val packageInfo = mock<PackageInfo>()
        packageInfo.versionName = "version.name"
        val packageManager = mock<PackageManager>()
        `when`(packageManager.getPackageInfo(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val activeExperiments = fretboard.getActiveExperiments(context)
        assertEquals(2, activeExperiments.size)
        assertTrue(activeExperiments.any { it.id == "second-id" })
        assertTrue(activeExperiments.any { it.id == "third-id" })
    }

    @Test
    fun getExperimentsMap() {
        val experimentSource = mock<ExperimentSource>()
        val experimentStorage = mock<ExperimentStorage>()
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

        val context = mock<Context>()
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPrefs = mock<SharedPreferences>()
        val prefsEditor = mock(SharedPreferences.Editor::class.java)
        `when`(prefsEditor.putString(ArgumentMatchers.anyString(), ArgumentMatchers.anyString())).thenReturn(prefsEditor)
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(sharedPrefs.getBoolean(ArgumentMatchers.anyString(), ArgumentMatchers.anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(sharedPrefs)

        val packageInfo = mock<PackageInfo>()
        packageInfo.versionName = "version.name"
        val packageManager = mock<PackageManager>()
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
    fun isInExperiment() {
        val experimentSource = mock<ExperimentSource>()
        val experimentStorage = mock<ExperimentStorage>()
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

        val context = mock<Context>()
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPrefs = mock<SharedPreferences>()
        val prefsEditor = mock(SharedPreferences.Editor::class.java)
        `when`(prefsEditor.putString(ArgumentMatchers.anyString(), ArgumentMatchers.anyString())).thenReturn(prefsEditor)
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(sharedPrefs.getBoolean(ArgumentMatchers.anyString(), ArgumentMatchers.anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(sharedPrefs)

        val packageInfo = mock<PackageInfo>()
        packageInfo.versionName = "version.name"
        val packageManager = mock<PackageManager>()
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
    fun withExperiment() {
        val experimentSource = mock<ExperimentSource>()
        val experimentStorage = mock<ExperimentStorage>()
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

        val context = mock<Context>()
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPrefs = mock<SharedPreferences>()
        val prefsEditor = mock(SharedPreferences.Editor::class.java)
        `when`(prefsEditor.putString(ArgumentMatchers.anyString(), ArgumentMatchers.anyString())).thenReturn(prefsEditor)
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(sharedPrefs.getBoolean(ArgumentMatchers.anyString(), ArgumentMatchers.anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(sharedPrefs)

        val packageInfo = mock<PackageInfo>()
        packageInfo.versionName = "version.name"
        val packageManager = mock<PackageManager>()
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
    fun getExperiment() {
        val experimentSource = mock<ExperimentSource>()
        val experimentStorage = mock<ExperimentStorage>()
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
    fun setOverride() {
        val experimentSource = mock<ExperimentSource>()
        val experimentStorage = mock<ExperimentStorage>()
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

        val context = mock<Context>()
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPrefs = mock<SharedPreferences>()
        val prefsEditor = mock(SharedPreferences.Editor::class.java)
        `when`(prefsEditor.putBoolean(ArgumentMatchers.anyString(), ArgumentMatchers.anyBoolean())).thenReturn(prefsEditor)
        `when`(prefsEditor.putString(ArgumentMatchers.anyString(), ArgumentMatchers.anyString())).thenReturn(prefsEditor)
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(sharedPrefs.getBoolean(ArgumentMatchers.anyString(), ArgumentMatchers.anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(sharedPrefs)

        val packageInfo = mock<PackageInfo>()
        packageInfo.versionName = "version.name"
        val packageManager = mock<PackageManager>()
        `when`(packageManager.getPackageInfo(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        assertTrue(fretboard.isInExperiment(context, ExperimentDescriptor("first-name")))
        fretboard.setOverride(context, ExperimentDescriptor("first-name"), false)
        verify(prefsEditor).putBoolean("first-name", false)
        fretboard.setOverride(context, ExperimentDescriptor("first-name"), true)
        verify(prefsEditor).putBoolean("first-name", true)

        runBlocking(Dispatchers.Default) {
            assertTrue(fretboard.isInExperiment(context, ExperimentDescriptor("first-name")))
            fretboard.setOverrideNow(context, ExperimentDescriptor("first-name"), false)
            verify(prefsEditor, times(2)).putBoolean("first-name", false)
            fretboard.setOverrideNow(context, ExperimentDescriptor("first-name"), true)
            verify(prefsEditor, times(2)).putBoolean("first-name", true)
        }
    }

    @Test
    fun clearOverride() {
        val experimentSource = mock<ExperimentSource>()
        val experimentStorage = mock<ExperimentStorage>()
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

        val context = mock<Context>()
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPrefs = mock<SharedPreferences>()
        val prefsEditor = mock(SharedPreferences.Editor::class.java)
        `when`(prefsEditor.remove(ArgumentMatchers.anyString())).thenReturn(prefsEditor)
        `when`(prefsEditor.putBoolean(ArgumentMatchers.anyString(), ArgumentMatchers.anyBoolean())).thenReturn(prefsEditor)
        `when`(prefsEditor.putString(ArgumentMatchers.anyString(), ArgumentMatchers.anyString())).thenReturn(prefsEditor)
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(sharedPrefs.getBoolean(ArgumentMatchers.anyString(), ArgumentMatchers.anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(sharedPrefs)

        val packageInfo = mock<PackageInfo>()
        packageInfo.versionName = "version.name"
        val packageManager = mock<PackageManager>()
        `when`(packageManager.getPackageInfo(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        assertTrue(fretboard.isInExperiment(context, ExperimentDescriptor("first-name")))
        fretboard.setOverride(context, ExperimentDescriptor("first-name"), false)
        fretboard.clearOverride(context, ExperimentDescriptor("first-name"))
        verify(prefsEditor).remove("first-name")

        runBlocking(Dispatchers.Default) {
            fretboard.setOverrideNow(context, ExperimentDescriptor("first-name"), false)
            fretboard.clearOverrideNow(context, ExperimentDescriptor("first-name"))
            verify(prefsEditor, times(2)).remove("first-name")
        }
    }

    @Test
    fun clearAllOverrides() {
        val experimentSource = mock<ExperimentSource>()
        val experimentStorage = mock<ExperimentStorage>()
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

        val context = mock<Context>()
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPrefs = mock<SharedPreferences>()
        val prefsEditor = mock(SharedPreferences.Editor::class.java)
        `when`(prefsEditor.clear()).thenReturn(prefsEditor)
        `when`(prefsEditor.putBoolean(ArgumentMatchers.anyString(), ArgumentMatchers.anyBoolean())).thenReturn(prefsEditor)
        `when`(prefsEditor.putString(ArgumentMatchers.anyString(), ArgumentMatchers.anyString())).thenReturn(prefsEditor)
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(sharedPrefs.getBoolean(ArgumentMatchers.anyString(), ArgumentMatchers.anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(sharedPrefs)

        val packageInfo = mock<PackageInfo>()
        packageInfo.versionName = "version.name"
        val packageManager = mock<PackageManager>()
        `when`(packageManager.getPackageInfo(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        assertTrue(fretboard.isInExperiment(context, ExperimentDescriptor("first-name")))
        fretboard.setOverride(context, ExperimentDescriptor("first-name"), false)
        fretboard.clearAllOverrides(context)
        verify(prefsEditor).clear()

        runBlocking(Dispatchers.Default) {
            fretboard.setOverrideNow(context, ExperimentDescriptor("first-name"), false)
            fretboard.clearAllOverridesNow(context)
            verify(prefsEditor, times(2)).clear()
        }
    }

    @Test
    fun updateExperimentsException() {
        val source = mock<ExperimentSource>()
        doAnswer {
            throw ExperimentDownloadException("test")
        }.`when`(source).getExperiments(any())
        val storage = mock<ExperimentStorage>()
        `when`(storage.retrieve()).thenReturn(ExperimentsSnapshot(listOf(), null))
        val fretboard = Fretboard(source, storage)
        fretboard.updateExperiments()
    }

    @Test
    fun getUserBucket() {
        val context = mock<Context>()
        val sharedPrefs = mock<SharedPreferences>()
        val prefsEditor = mock(SharedPreferences.Editor::class.java)
        val experimentSource = mock<ExperimentSource>()
        val experimentStorage = mock<ExperimentStorage>()
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(prefsEditor.putBoolean(ArgumentMatchers.anyString(), ArgumentMatchers.anyBoolean())).thenReturn(prefsEditor)
        `when`(prefsEditor.putString(ArgumentMatchers.anyString(), ArgumentMatchers.anyString())).thenReturn(prefsEditor)
        `when`(context.getSharedPreferences(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(sharedPrefs)
        `when`(sharedPrefs.getString(ArgumentMatchers.anyString(), ArgumentMatchers.isNull()))
                .thenReturn("a94b1dab-030e-4b13-be15-cc80c1eda8b3")
        val fretboard = Fretboard(experimentSource, experimentStorage)
        assertTrue(fretboard.getUserBucket(context) == 54)
    }

    @Test
    fun getUserBucketWithOverridenClientId() {
        val experimentSource = mock<ExperimentSource>()
        val experimentStorage = mock<ExperimentStorage>()

        val fretboard1 = Fretboard(experimentSource, experimentStorage, object : ValuesProvider() {
            override fun getClientId(context: Context): String = "c641eacf-c30c-4171-b403-f077724e848a"
        })

        assertEquals(79, fretboard1.getUserBucket(testContext))

        val fretboard2 = Fretboard(experimentSource, experimentStorage, object : ValuesProvider() {
            override fun getClientId(context: Context): String = "01a15650-9a5d-4383-a7ba-2f047b25c620"
        })

        assertEquals(55, fretboard2.getUserBucket(testContext))
    }

    @Test
    fun evenDistribution() {
        val context = mock<Context>()
        val sharedPrefs = mock<SharedPreferences>()
        val prefsEditor = mock(SharedPreferences.Editor::class.java)
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(prefsEditor.putBoolean(ArgumentMatchers.anyString(), ArgumentMatchers.anyBoolean())).thenReturn(prefsEditor)
        `when`(prefsEditor.putString(ArgumentMatchers.anyString(), ArgumentMatchers.anyString())).thenReturn(prefsEditor)
        `when`(context.getSharedPreferences(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(sharedPrefs)

        val distribution = (1..1000).map {
            val experimentEvaluator = ExperimentEvaluator()
            val f = experimentEvaluator::class.functions.find { it.name == "getUserBucket" }
            f!!.isAccessible = true
            f.call(experimentEvaluator, context) as Int
        }

        distribution
                .groupingBy { it }
                .eachCount()
                .forEach {
                    assertTrue(it.value in 0..25)
                }

        distribution
                .groupingBy { it / 10 }
                .eachCount()
                .forEach {
                    assertTrue(it.value in 50..150)
                }

        distribution
                .groupingBy { it / 50 }
                .eachCount()
                .forEach {
                    assertTrue(it.value in 350..650)
                }
    }

    @Test
    fun loadingCorruptJSON() {
        val experimentSource = mock<ExperimentSource>()

        val file = File(testContext.filesDir, "corrupt-experiments.json")
        file.writer().use {
            it.write("""{"experiment":[""")
        }

        val experimentStorage = FlatFileExperimentStorage(file)

        val fretboard = Fretboard(experimentSource, experimentStorage)
        fretboard.loadExperiments() // Should not throw
    }
}