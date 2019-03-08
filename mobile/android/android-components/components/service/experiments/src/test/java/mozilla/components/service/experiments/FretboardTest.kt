/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

import android.content.Context
import android.content.SharedPreferences
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.runBlocking
import mozilla.components.service.experiments.storage.flatfile.FlatFileExperimentStorage
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
import org.robolectric.RuntimeEnvironment
import java.io.File
import kotlin.reflect.full.functions
import kotlin.reflect.jvm.isAccessible

@RunWith(RobolectricTestRunner::class)
class ExperimentsTest {
    @Test
    fun loadExperiments() {
        val experimentSource = mock(ExperimentSource::class.java)
        val experimentStorage = mock(ExperimentStorage::class.java)
        val instance = Experiments(experimentSource, experimentStorage)
        instance.loadExperiments()
        verify(experimentStorage).retrieve()
    }

    @Test
    fun updateExperimentsStorageNotLoaded() {
        val experimentSource = mock(ExperimentSource::class.java)
        val experimentStorage = mock(ExperimentStorage::class.java)
        val instance = Experiments(experimentSource, experimentStorage)
        instance.updateExperiments()
        verify(experimentStorage, times(1)).retrieve()
        instance.updateExperiments()
        verify(experimentStorage, times(1)).retrieve()
    }

    @Test
    fun updateExperimentsEmptyStorage() {
        val experimentSource = mock(ExperimentSource::class.java)
        val result = ExperimentsSnapshot(listOf(), null)
        `when`(experimentSource.getExperiments(result)).thenReturn(ExperimentsSnapshot(listOf(Experiment("id", "name")), null))
        val experimentStorage = mock(ExperimentStorage::class.java)
        `when`(experimentStorage.retrieve()).thenReturn(result)
        val instance = Experiments(experimentSource, experimentStorage)
        instance.updateExperiments()
        verify(experimentSource).getExperiments(result)
        verify(experimentStorage).save(ExperimentsSnapshot(listOf(Experiment("id", "name")), null))
    }

    @Test
    fun updateExperimentsFromStorage() {
        val experimentSource = mock(ExperimentSource::class.java)
        `when`(experimentSource.getExperiments(ExperimentsSnapshot(listOf(Experiment("id0", "name0")), null))).thenReturn(ExperimentsSnapshot(listOf(Experiment("id", "name")), null))
        val experimentStorage = mock(ExperimentStorage::class.java)
        `when`(experimentStorage.retrieve()).thenReturn(ExperimentsSnapshot(listOf(Experiment("id0", "name0")), null))
        val instance = Experiments(experimentSource, experimentStorage)
        instance.updateExperiments()
        verify(experimentSource).getExperiments(ExperimentsSnapshot(listOf(Experiment("id0", "name0")), null))
        verify(experimentStorage).save(ExperimentsSnapshot(listOf(Experiment("id", "name")), null))
    }

    @Test
    fun experiments() {
        val experimentSource = mock(ExperimentSource::class.java)
        val experimentStorage = mock(ExperimentStorage::class.java)
        val experiments = listOf(
            Experiment("first-id", "first-name"),
            Experiment("second-id", "second-name")
        )
        `when`(experimentStorage.retrieve()).thenReturn(ExperimentsSnapshot(experiments, null))
        val instance = Experiments(experimentSource, experimentStorage)
        var returnedExperiments = instance.experiments
        assertEquals(0, returnedExperiments.size)
        instance.loadExperiments()
        returnedExperiments = instance.experiments
        assertEquals(2, returnedExperiments.size)
        assertTrue(returnedExperiments.contains(experiments[0]))
        assertTrue(returnedExperiments.contains(experiments[1]))
    }

    @Test
    fun experimentsNoExperiments() {
        val experimentSource = mock(ExperimentSource::class.java)
        val experimentStorage = mock(ExperimentStorage::class.java)
        val experiments = listOf<Experiment>()
        `when`(experimentStorage.retrieve()).thenReturn(ExperimentsSnapshot(experiments, null))
        val instance = Experiments(experimentSource, experimentStorage)
        val returnedExperiments = instance.experiments
        assertEquals(0, returnedExperiments.size)
    }

    @Test
    fun getActiveExperiments() {
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
        val instance = Experiments(experimentSource, experimentStorage)
        instance.loadExperiments()

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

        val activeExperiments = instance.getActiveExperiments(context)
        assertEquals(2, activeExperiments.size)
        assertTrue(activeExperiments.any { it.id == "second-id" })
        assertTrue(activeExperiments.any { it.id == "third-id" })
    }

    @Test
    fun getExperimentsMap() {
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
        val instance = Experiments(experimentSource, experimentStorage)
        instance.loadExperiments()

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

        val experimentsMap = instance.getExperimentsMap(context)
        assertEquals(3, experimentsMap.size)
        println(experimentsMap.toString())
        assertTrue(experimentsMap["first-name"] == false)
        assertTrue(experimentsMap["second-name"] == true)
        assertTrue(experimentsMap["third-name"] == true)
    }

    @Test
    fun isInExperiment() {
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
        var instance = Experiments(experimentSource, experimentStorage)
        instance.loadExperiments()

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

        assertTrue(instance.isInExperiment(context, ExperimentDescriptor("first-name")))

        experiments = listOf(
            Experiment("first-id",
                name = "first-name",
                match = Experiment.Matcher(
                    appId = "other.appId"
                )
            )
        )
        `when`(experimentStorage.retrieve()).thenReturn(ExperimentsSnapshot(experiments, null))
        instance = Experiments(experimentSource, experimentStorage)

        assertFalse(instance.isInExperiment(context, ExperimentDescriptor("first-name")))
        assertFalse(instance.isInExperiment(context, ExperimentDescriptor("other-name")))
    }

    @Test
    fun withExperiment() {
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
        var instance = Experiments(experimentSource, experimentStorage)
        instance.loadExperiments()

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
        instance.withExperiment(context, ExperimentDescriptor("first-name")) {
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
        instance = Experiments(experimentSource, experimentStorage)

        invocations = 0
        instance.withExperiment(context, ExperimentDescriptor("first-name")) {
            invocations++
        }
        assertEquals(0, invocations)

        invocations = 0
        instance.withExperiment(context, ExperimentDescriptor("other-name")) {
            invocations++
        }
        assertEquals(0, invocations)
    }

    @Test
    fun getExperiment() {
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
        val instance = Experiments(experimentSource, experimentStorage)
        instance.loadExperiments()

        assertEquals(experiments[0], instance.getExperiment(ExperimentDescriptor("first-name")))
        assertNull(instance.getExperiment(ExperimentDescriptor("other-name")))
    }

    @Test
    fun setOverride() {
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
        val instance = Experiments(experimentSource, experimentStorage)
        instance.loadExperiments()

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

        assertTrue(instance.isInExperiment(context, ExperimentDescriptor("first-name")))
        instance.setOverride(context, ExperimentDescriptor("first-name"), false)
        verify(prefsEditor).putBoolean("first-name", false)
        instance.setOverride(context, ExperimentDescriptor("first-name"), true)
        verify(prefsEditor).putBoolean("first-name", true)

        runBlocking(Dispatchers.Default) {
            assertTrue(instance.isInExperiment(context, ExperimentDescriptor("first-name")))
            instance.setOverrideNow(context, ExperimentDescriptor("first-name"), false)
            verify(prefsEditor, times(2)).putBoolean("first-name", false)
            instance.setOverrideNow(context, ExperimentDescriptor("first-name"), true)
            verify(prefsEditor, times(2)).putBoolean("first-name", true)
        }
    }

    @Test
    fun clearOverride() {
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
        val instance = Experiments(experimentSource, experimentStorage)
        instance.loadExperiments()

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

        assertTrue(instance.isInExperiment(context, ExperimentDescriptor("first-name")))
        instance.setOverride(context, ExperimentDescriptor("first-name"), false)
        instance.clearOverride(context, ExperimentDescriptor("first-name"))
        verify(prefsEditor).remove("first-name")

        runBlocking(Dispatchers.Default) {
            instance.setOverrideNow(context, ExperimentDescriptor("first-name"), false)
            instance.clearOverrideNow(context, ExperimentDescriptor("first-name"))
            verify(prefsEditor, times(2)).remove("first-name")
        }
    }

    @Test
    fun clearAllOverrides() {
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
        val instance = Experiments(experimentSource, experimentStorage)
        instance.loadExperiments()

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

        assertTrue(instance.isInExperiment(context, ExperimentDescriptor("first-name")))
        instance.setOverride(context, ExperimentDescriptor("first-name"), false)
        instance.clearAllOverrides(context)
        verify(prefsEditor).clear()

        runBlocking(Dispatchers.Default) {
            instance.setOverrideNow(context, ExperimentDescriptor("first-name"), false)
            instance.clearAllOverridesNow(context)
            verify(prefsEditor, times(2)).clear()
        }
    }

    @Test
    fun updateExperimentsException() {
        val source = mock(ExperimentSource::class.java)
        doAnswer {
            throw ExperimentDownloadException("test")
        }.`when`(source).getExperiments(any())
        val storage = mock(ExperimentStorage::class.java)
        `when`(storage.retrieve()).thenReturn(ExperimentsSnapshot(listOf(), null))
        val instance = Experiments(source, storage)
        instance.updateExperiments()
    }

    @Test
    fun getUserBucket() {
        val context = mock(Context::class.java)
        val sharedPrefs = mock(SharedPreferences::class.java)
        val prefsEditor = mock(SharedPreferences.Editor::class.java)
        val experimentSource = mock(ExperimentSource::class.java)
        val experimentStorage = mock(ExperimentStorage::class.java)
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(prefsEditor.putBoolean(ArgumentMatchers.anyString(), ArgumentMatchers.anyBoolean())).thenReturn(prefsEditor)
        `when`(prefsEditor.putString(ArgumentMatchers.anyString(), ArgumentMatchers.anyString())).thenReturn(prefsEditor)
        `when`(context.getSharedPreferences(ArgumentMatchers.anyString(), ArgumentMatchers.anyInt())).thenReturn(sharedPrefs)
        `when`(sharedPrefs.getString(ArgumentMatchers.anyString(), ArgumentMatchers.isNull()))
                .thenReturn("a94b1dab-030e-4b13-be15-cc80c1eda8b3")
        val instance = Experiments(experimentSource, experimentStorage)
        assertTrue(instance.getUserBucket(context) == 54)
    }

    @Test
    fun getUserBucketWithOverridenClientId() {
        val experimentSource = mock(ExperimentSource::class.java)
        val experimentStorage = mock(ExperimentStorage::class.java)

        val instance1 = Experiments(experimentSource, experimentStorage, object : ValuesProvider() {
            override fun getClientId(context: Context): String = "c641eacf-c30c-4171-b403-f077724e848a"
        })

        assertEquals(79, instance1.getUserBucket(RuntimeEnvironment.application))

        val instance2 = Experiments(experimentSource, experimentStorage, object : ValuesProvider() {
            override fun getClientId(context: Context): String = "01a15650-9a5d-4383-a7ba-2f047b25c620"
        })

        assertEquals(55, instance2.getUserBucket(RuntimeEnvironment.application))
    }

    @Test
    fun evenDistribution() {
        val context = mock(Context::class.java)
        val sharedPrefs = mock(SharedPreferences::class.java)
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
        val experimentSource = mock(ExperimentSource::class.java)

        val file = File(RuntimeEnvironment.application.filesDir, "corrupt-experiments.json")
        file.writer().use {
            it.write("""{"experiment":[""")
        }

        val experimentStorage = FlatFileExperimentStorage(file)

        val instance = Experiments(experimentSource, experimentStorage)
        instance.loadExperiments() // Should not throw
    }
}