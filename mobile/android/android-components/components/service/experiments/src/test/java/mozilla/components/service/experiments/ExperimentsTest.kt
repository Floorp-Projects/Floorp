/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

import android.content.Context
import android.content.SharedPreferences
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.runBlocking
import mozilla.components.support.test.any
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import java.io.File
import mozilla.components.service.glean.GleanInternalAPI
import mozilla.components.support.test.mock
import org.mockito.ArgumentMatchers.anyString
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.ArgumentMatchers.isNull
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy
import org.mockito.Mockito.doAnswer
import org.mockito.Mockito.`when`
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyZeroInteractions
import org.mockito.Mockito.times

@RunWith(RobolectricTestRunner::class)
class ExperimentsTest {
    private var context: Context = ApplicationProvider.getApplicationContext()
    private var glean: GleanInternalAPI = mock()
    private lateinit var configuration: Configuration
    private lateinit var experiments: ExperimentsInternalAPI
    private lateinit var experimentStorage: FlatFileExperimentStorage
    private lateinit var experimentSource: KintoExperimentSource
    private lateinit var experimentsUpdater: ExperimentsUpdater

    private fun getDefaultExperimentStorageMock(): FlatFileExperimentStorage {
        val storage: FlatFileExperimentStorage = mock()
        `when`(storage.retrieve()).thenReturn(ExperimentsSnapshot(emptyList(), null))
        return storage
    }

    private fun getDefaultExperimentSourceMock(): KintoExperimentSource {
        val source: KintoExperimentSource = mock()
        `when`(source.getExperiments(any())).thenReturn(ExperimentsSnapshot(emptyList(), null))
        return source
    }

    private fun resetExperiments(
        source: KintoExperimentSource = getDefaultExperimentSourceMock(),
        storage: FlatFileExperimentStorage = getDefaultExperimentStorageMock(),
        valuesProvider: ValuesProvider = ValuesProvider()
    ) {
        configuration = Configuration()
        experiments = spy(ExperimentsInternalAPI())
        experiments.valuesProvider = valuesProvider

        `when`(glean.isInitialized()).thenReturn(true)
        `when`(experiments.getGlean()).thenReturn(glean)
        experimentStorage = storage
        `when`(experiments.getExperimentsStorage(context)).thenReturn(storage)

        experimentsUpdater = spy(ExperimentsUpdater(context, experiments))
        `when`(experiments.getExperimentsUpdater(context)).thenReturn(experimentsUpdater)
        experimentSource = source
        `when`(experimentsUpdater.getExperimentSource(configuration)).thenReturn(experimentSource)
    }

    @Test
    fun `initialize experiments`() {
        resetExperiments()

        experiments.initialize(context, configuration)
        runBlocking {
            experimentsUpdater.testWaitForUpdate()
        }

        verify(glean).isInitialized()
        verify(experimentStorage).retrieve()
        verify(experimentsUpdater).initialize(any())
        verify(experimentSource).getExperiments(any())
    }

    @Test
    fun `initialize experiments before glean`() {
        resetExperiments()

        `when`(glean.isInitialized()).thenReturn(false)
        experiments.initialize(context, configuration)

        // Glean was not initialized yet, so no other initialization should have happened.
        verify(glean).isInitialized()
        assertEquals(experiments.isInitialized, false)
        verifyZeroInteractions(experimentStorage)
        verifyZeroInteractions(experimentsUpdater)
        verifyZeroInteractions(experimentSource)
    }

    @Test
    fun `async updating of experiments on library init`() {
        resetExperiments()

        // Set up the Kinto-side experiments.
        val experimentsList = listOf(Experiment("id", "name"))
        val snapshot = ExperimentsSnapshot(experimentsList, null)
        `when`(experimentSource.getExperiments(any())).thenReturn(snapshot)

        experiments.initialize(context, configuration)
        // Wait for async update to finish.
        runBlocking(Dispatchers.Default) {
            experimentsUpdater.testWaitForUpdate()
        }

        verify(experimentStorage, times(1)).retrieve()
        verify(experimentSource, times(1)).getExperiments(any())
        verify(experiments, times(1)).onExperimentsUpdated(snapshot)
    }

    @Test
    fun `update experiments from empty storage`() {
        resetExperiments()
        val emptySnapshot = ExperimentsSnapshot(listOf(), null)
        val updateResult = ExperimentsSnapshot(listOf(Experiment("id", "name")), null)
        `when`(experimentSource.getExperiments(emptySnapshot)).thenReturn(updateResult)
        `when`(experimentStorage.retrieve()).thenReturn(emptySnapshot)

        experiments.initialize(context, configuration)
        runBlocking {
            experimentsUpdater.testWaitForUpdate()
        }

        verify(experimentSource).getExperiments(emptySnapshot)
        verify(experiments).onExperimentsUpdated(updateResult)
        verify(experimentStorage).save(updateResult)
    }

    @Test
    fun `update experiments from non-empty storage`() {
        resetExperiments()

        val storageSnapshot = ExperimentsSnapshot(listOf(Experiment("id0", "name0")), null)
        val updateResult = ExperimentsSnapshot(listOf(Experiment("id", "name")), null)
        `when`(experimentSource.getExperiments(storageSnapshot)).thenReturn(updateResult)
        `when`(experimentStorage.retrieve()).thenReturn(storageSnapshot)

        experiments.initialize(context, configuration)
        runBlocking {
            experimentsUpdater.testWaitForUpdate()
        }

        verify(experimentSource).getExperiments(storageSnapshot)
        verify(experiments).onExperimentsUpdated(updateResult)
        verify(experimentStorage).save(updateResult)
    }

    @Test
    fun `load two experiments from storage`() {
        resetExperiments()
        val experimentsList = listOf(
            Experiment("first-id", "first-name"),
            Experiment("second-id", "second-name")
        )
        `when`(experimentStorage.retrieve()).thenReturn(ExperimentsSnapshot(experimentsList, null))
        // Throwing from the Kinto update means we continue with the experiments loaded from
        // storage.
        `when`(experimentSource.getExperiments(any())).then {
            throw ExperimentDownloadException("oops")
        }
        experiments.initialize(context, configuration)
        runBlocking {
            experimentsUpdater.testWaitForUpdate()
        }

        val returned = experiments.experiments
        assertEquals(2, returned.size)
        assertTrue(returned.contains(experimentsList[0]))
        assertTrue(returned.contains(experimentsList[1]))
    }

    @Test
    fun `load empty experiments list from storage`() {
        resetExperiments()
        val experimentsList = emptyList<Experiment>()
        `when`(experimentStorage.retrieve()).thenReturn(ExperimentsSnapshot(experimentsList, null))
        // Throwing from the Kinto update means we continue with the experiments loaded from
        // storage.
        `when`(experimentSource.getExperiments(any())).then {
            throw ExperimentDownloadException("oops")
        }
        experiments.initialize(context, configuration)
        runBlocking {
            experimentsUpdater.testWaitForUpdate()
        }

        val returnedExperiments = experiments.experiments
        assertEquals(0, returnedExperiments.size)
    }

    @Test
    fun `load empty experiments list from server`() {
        resetExperiments()
        val experimentsList = emptyList<Experiment>()
        val snapshot = ExperimentsSnapshot(experimentsList, null)
        `when`(experimentStorage.retrieve()).thenReturn(snapshot)
        `when`(experimentSource.getExperiments(any())).thenReturn(snapshot)
        experiments.initialize(context, configuration)
        runBlocking {
            experimentsUpdater.testWaitForUpdate()
        }

        val returnedExperiments = experiments.experiments
        assertEquals(0, returnedExperiments.size)
    }

    @Test
    fun `second update of experiments`() {
        resetExperiments()

        // Set up the Kinto-side experiments.
        val experimentsList1 = listOf(Experiment("id", "name"))
        val snapshot1 = ExperimentsSnapshot(experimentsList1, null)
        `when`(experimentSource.getExperiments(any())).thenReturn(snapshot1)

        experiments.initialize(context, configuration)
        runBlocking {
            experimentsUpdater.testWaitForUpdate()
        }

        verify(experimentStorage, times(1)).retrieve()
        verify(experimentSource, times(1)).getExperiments(any())
        verify(experiments, times(1)).onExperimentsUpdated(snapshot1)
        assertEquals(experimentsList1, experiments.experiments)
        assertEquals(snapshot1, experiments.experimentsResult)

        // Now change the source list, trigger an update and check it propagated.
        val experimentsList2 = listOf(Experiment("id2", "name2"))
        val snapshot2 = ExperimentsSnapshot(experimentsList2, null)
        `when`(experimentSource.getExperiments(any())).thenReturn(snapshot2)

        experimentsUpdater.updateExperiments()
        verify(experimentStorage, times(1)).retrieve()
        verify(experimentSource, times(1)).getExperiments(snapshot1)
        verify(experiments, times(1)).onExperimentsUpdated(snapshot2)
        assertEquals(experimentsList2, experiments.experiments)
        assertEquals(snapshot2, experiments.experimentsResult)
    }

    @Test
    fun getActiveExperiments() {
        resetExperiments()
        val experimentsList = listOf(
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
        `when`(experimentSource.getExperiments(any())).thenReturn(ExperimentsSnapshot(experimentsList, null))
        experiments.initialize(context, configuration)
        runBlocking {
            experimentsUpdater.testWaitForUpdate()
        }

        val context = mock(Context::class.java)
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPrefs = mock(SharedPreferences::class.java)
        val prefsEditor = mock(SharedPreferences.Editor::class.java)
        `when`(prefsEditor.putString(anyString(), anyString())).thenReturn(prefsEditor)
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(sharedPrefs.getBoolean(anyString(), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), anyInt())).thenReturn(sharedPrefs)

        val packageInfo = mock(PackageInfo::class.java)
        packageInfo.versionName = "version.name"
        val packageManager = mock(PackageManager::class.java)
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val activeExperiments = experiments.getActiveExperiments(context)
        assertEquals(2, activeExperiments.size)
        assertTrue(activeExperiments.any { it.id == "second-id" })
        assertTrue(activeExperiments.any { it.id == "third-id" })
    }

    @Test
    fun getExperimentsMap() {
        resetExperiments()
        val experimentsList = listOf(
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
        `when`(experimentSource.getExperiments(any())).thenReturn(ExperimentsSnapshot(experimentsList, null))
        experiments.initialize(context, configuration)
        runBlocking {
            experimentsUpdater.testWaitForUpdate()
        }

        val context = mock(Context::class.java)
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPrefs = mock(SharedPreferences::class.java)
        val prefsEditor = mock(SharedPreferences.Editor::class.java)
        `when`(prefsEditor.putString(anyString(), anyString())).thenReturn(prefsEditor)
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(sharedPrefs.getBoolean(anyString(), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), anyInt())).thenReturn(sharedPrefs)

        val packageInfo = mock(PackageInfo::class.java)
        packageInfo.versionName = "version.name"
        val packageManager = mock(PackageManager::class.java)
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val experimentsMap = experiments.getExperimentsMap(context)
        assertEquals(3, experimentsMap.size)
        println(experimentsMap.toString())
        assertTrue(experimentsMap["first-name"] == false)
        assertTrue(experimentsMap["second-name"] == true)
        assertTrue(experimentsMap["third-name"] == true)
    }

    @Test
    fun isInExperiment() {
        resetExperiments()
        var experimentsList = listOf(
            Experiment("first-id",
                name = "first-name",
                match = Experiment.Matcher(
                    appId = "test.appId"
                )
            )
        )
        `when`(experimentSource.getExperiments(any())).thenReturn(ExperimentsSnapshot(experimentsList, null))
        experiments.initialize(context, configuration)
        runBlocking {
            experimentsUpdater.testWaitForUpdate()
        }

        val context = mock(Context::class.java)
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPrefs = mock(SharedPreferences::class.java)
        val prefsEditor = mock(SharedPreferences.Editor::class.java)
        `when`(prefsEditor.putString(anyString(), anyString())).thenReturn(prefsEditor)
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(sharedPrefs.getBoolean(anyString(), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), anyInt())).thenReturn(sharedPrefs)

        val packageInfo = mock(PackageInfo::class.java)
        packageInfo.versionName = "version.name"
        val packageManager = mock(PackageManager::class.java)
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        assertTrue(experiments.isInExperiment(context, "first-name"))

        // Update experiments with the experiment "first-id" not being active anymore.
        experimentsList = listOf(
            Experiment("first-id",
                name = "first-name",
                match = Experiment.Matcher(
                    appId = "other.appId"
                )
            )
        )
        `when`(experimentSource.getExperiments(any())).thenReturn(ExperimentsSnapshot(experimentsList, null))
        experimentsUpdater.updateExperiments()

        assertFalse(experiments.isInExperiment(context, "first-name"))
        assertFalse(experiments.isInExperiment(context, "other-name"))
    }

    @Test
    fun withExperiment() {
        resetExperiments()
        var experimentsList = listOf(
            Experiment("first-id",
                name = "first-name",
                match = Experiment.Matcher(
                    appId = "test.appId"
                )
            )
        )
        `when`(experimentSource.getExperiments(any())).thenReturn(ExperimentsSnapshot(experimentsList, null))

        val mockContext = mock(Context::class.java)
        `when`(mockContext.packageName).thenReturn("test.appId")
        val sharedPrefs = mock(SharedPreferences::class.java)
        val prefsEditor = mock(SharedPreferences.Editor::class.java)
        `when`(prefsEditor.putString(anyString(), anyString())).thenReturn(prefsEditor)
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(sharedPrefs.getBoolean(anyString(), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(mockContext.getSharedPreferences(anyString(), anyInt())).thenReturn(sharedPrefs)

        val packageInfo = mock(PackageInfo::class.java)
        packageInfo.versionName = "version.name"
        val packageManager = mock(PackageManager::class.java)
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(mockContext.packageManager).thenReturn(packageManager)

        experiments.initialize(context, configuration)
        runBlocking {
            experimentsUpdater.testWaitForUpdate()
        }

        var invocations = 0
        experiments.withExperiment(mockContext, "first-name") {
            invocations++
        }
        assertEquals(1, invocations)

        // Change experiments list and trigger an update. The matcher now contains a non-matching
        // app id, so no experiments should be active anymore.
        experimentsList = listOf(
            Experiment("first-id",
                name = "first-name",
                match = Experiment.Matcher(
                    appId = "other.appId"
                )
            )
        )
        `when`(experimentSource.getExperiments(any())).thenReturn(ExperimentsSnapshot(experimentsList, null))
        experimentsUpdater.updateExperiments()

        invocations = 0
        experiments.withExperiment(mockContext, "first-name") {
            invocations++
        }
        assertEquals(0, invocations)

        invocations = 0
        experiments.withExperiment(mockContext, "other-name") {
            invocations++
        }
        assertEquals(0, invocations)
    }

    @Test
    fun getExperiment() {
        resetExperiments()
        val experimentsList = listOf(
            Experiment("first-id",
                name = "first-name",
                match = Experiment.Matcher(
                    appId = "test.appId"
                )
            )
        )
        `when`(experimentSource.getExperiments(any())).thenReturn(ExperimentsSnapshot(experimentsList, null))
        experiments.initialize(context, configuration)
        runBlocking {
            experimentsUpdater.testWaitForUpdate()
        }

        assertEquals(experimentsList[0], experiments.getExperiment("first-name"))
        assertNull(experiments.getExperiment("other-name"))
    }

    @Test
    fun setOverride() {
        resetExperiments()
        val experimentsList = listOf(
            Experiment("first-id",
                name = "first-name",
                match = Experiment.Matcher(
                    appId = "test.appId"
                )
            )
        )
        `when`(experimentSource.getExperiments(any())).thenReturn(ExperimentsSnapshot(experimentsList, null))
        experiments.initialize(context, configuration)
        runBlocking {
            experimentsUpdater.testWaitForUpdate()
        }

        val context = mock(Context::class.java)
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPrefs = mock(SharedPreferences::class.java)
        val prefsEditor = mock(SharedPreferences.Editor::class.java)
        `when`(prefsEditor.putBoolean(anyString(), anyBoolean())).thenReturn(prefsEditor)
        `when`(prefsEditor.putString(anyString(), anyString())).thenReturn(prefsEditor)
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(sharedPrefs.getBoolean(anyString(), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), anyInt())).thenReturn(sharedPrefs)

        val packageInfo = mock(PackageInfo::class.java)
        packageInfo.versionName = "version.name"
        val packageManager = mock(PackageManager::class.java)
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        assertTrue(experiments.isInExperiment(context, "first-name"))
        experiments.setOverride(context, "first-name", false)
        verify(prefsEditor).putBoolean("first-name", false)
        experiments.setOverride(context, "first-name", true)
        verify(prefsEditor).putBoolean("first-name", true)

        runBlocking(Dispatchers.Default) {
            assertTrue(experiments.isInExperiment(context, "first-name"))
            experiments.setOverrideNow(context, "first-name", false)
            verify(prefsEditor, times(2)).putBoolean("first-name", false)
            experiments.setOverrideNow(context, "first-name", true)
            verify(prefsEditor, times(2)).putBoolean("first-name", true)
        }
    }

    @Test
    fun clearOverride() {
        resetExperiments()
        val experimentsList = listOf(
            Experiment("first-id",
                name = "first-name",
                match = Experiment.Matcher(
                    appId = "test.appId"
                )
            )
        )
        `when`(experimentSource.getExperiments(any())).thenReturn(ExperimentsSnapshot(experimentsList, null))
        experiments.initialize(context, configuration)
        runBlocking {
            experimentsUpdater.testWaitForUpdate()
        }

        val context = mock(Context::class.java)
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPrefs = mock(SharedPreferences::class.java)
        val prefsEditor = mock(SharedPreferences.Editor::class.java)
        `when`(prefsEditor.remove(anyString())).thenReturn(prefsEditor)
        `when`(prefsEditor.putBoolean(anyString(), anyBoolean())).thenReturn(prefsEditor)
        `when`(prefsEditor.putString(anyString(), anyString())).thenReturn(prefsEditor)
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(sharedPrefs.getBoolean(anyString(), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), anyInt())).thenReturn(sharedPrefs)

        val packageInfo = mock(PackageInfo::class.java)
        packageInfo.versionName = "version.name"
        val packageManager = mock(PackageManager::class.java)
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        assertTrue(experiments.isInExperiment(context, "first-name"))
        experiments.setOverride(context, "first-name", false)
        experiments.clearOverride(context, "first-name")
        verify(prefsEditor).remove("first-name")

        runBlocking(Dispatchers.Default) {
            experiments.setOverrideNow(context, "first-name", false)
            experiments.clearOverrideNow(context, "first-name")
            verify(prefsEditor, times(2)).remove("first-name")
        }
    }

    @Test
    fun clearAllOverrides() {
        resetExperiments()
        val experimentsList = listOf(
            Experiment("first-id",
                name = "first-name",
                match = Experiment.Matcher(
                    appId = "test.appId"
                )
            )
        )
        `when`(experimentSource.getExperiments(any())).thenReturn(ExperimentsSnapshot(experimentsList, null))
        experiments.initialize(context, configuration)
        runBlocking {
            experimentsUpdater.testWaitForUpdate()
        }

        val context = mock(Context::class.java)
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPrefs = mock(SharedPreferences::class.java)
        val prefsEditor = mock(SharedPreferences.Editor::class.java)
        `when`(prefsEditor.clear()).thenReturn(prefsEditor)
        `when`(prefsEditor.putBoolean(anyString(), anyBoolean())).thenReturn(prefsEditor)
        `when`(prefsEditor.putString(anyString(), anyString())).thenReturn(prefsEditor)
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(sharedPrefs.getBoolean(anyString(), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), anyInt())).thenReturn(sharedPrefs)

        val packageInfo = mock(PackageInfo::class.java)
        packageInfo.versionName = "version.name"
        val packageManager = mock(PackageManager::class.java)
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        assertTrue(experiments.isInExperiment(context, "first-name"))
        experiments.setOverride(context, "first-name", false)
        experiments.clearAllOverrides(context)
        verify(prefsEditor).clear()

        runBlocking(Dispatchers.Default) {
            experiments.setOverrideNow(context, "first-name", false)
            experiments.clearAllOverridesNow(context)
            verify(prefsEditor, times(2)).clear()
        }
    }

    @Test
    fun updateExperimentsException() {
        resetExperiments()
        doAnswer {
            throw ExperimentDownloadException("test")
        }.`when`(experimentSource).getExperiments(any())
        `when`(experimentStorage.retrieve()).thenReturn(ExperimentsSnapshot(listOf(), null))
        experiments.initialize(context, configuration)
        runBlocking {
            experimentsUpdater.testWaitForUpdate()
        }
    }

    @Test
    fun getUserBucket() {
        val mockContext = mock(Context::class.java)
        val sharedPrefs = mock(SharedPreferences::class.java)
        val prefsEditor = mock(SharedPreferences.Editor::class.java)

        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(prefsEditor.putBoolean(anyString(), anyBoolean())).thenReturn(prefsEditor)
        `when`(prefsEditor.putString(anyString(), anyString())).thenReturn(prefsEditor)
        `when`(mockContext.getSharedPreferences(anyString(), anyInt())).thenReturn(sharedPrefs)
        `when`(sharedPrefs.getString(anyString(), isNull()))
                .thenReturn("a94b1dab-030e-4b13-be15-cc80c1eda8b3")

        resetExperiments()
        experiments.initialize(context, configuration)
        runBlocking {
            experimentsUpdater.testWaitForUpdate()
        }
        assertTrue(experiments.getUserBucket(mockContext) == 54)
    }

    @Test
    fun `getUserBucket() with overriden clientId`() {
        val source = getDefaultExperimentSourceMock()
        val storage = getDefaultExperimentStorageMock()

        resetExperiments(source, storage, object : ValuesProvider() {
            override fun getClientId(context: Context): String = "c641eacf-c30c-4171-b403-f077724e848a"
        })
        experiments.initialize(context, configuration)
        runBlocking {
            experimentsUpdater.testWaitForUpdate()
        }

        assertEquals(79, experiments.getUserBucket(context))

        resetExperiments(source, storage, object : ValuesProvider() {
            override fun getClientId(context: Context): String = "01a15650-9a5d-4383-a7ba-2f047b25c620"
        })
        experiments.initialize(context, configuration)
        runBlocking {
            experimentsUpdater.testWaitForUpdate()
        }

        assertEquals(55, experiments.getUserBucket(context))
    }

    @Test
    fun `loading corrupt JSON`() {
        val experimentSource: KintoExperimentSource = mock()

        val file = File(RuntimeEnvironment.application.filesDir, "corrupt-experiments.json")
        file.writer().use {
            it.write("""{"experiment":[""")
        }

        val experimentStorage = FlatFileExperimentStorage(file)

        resetExperiments(experimentSource, experimentStorage)
        experiments.initialize(context, configuration)
        experiments.loadExperiments() // Should not throw
    }
}