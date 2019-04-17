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
import mozilla.components.support.test.eq
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

    private val EXAMPLE_CLIENT_ID = "c641eacf-c30c-4171-b403-f077724e848a"

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
        valuesProvider: ValuesProvider = object : ValuesProvider() {
            override fun getClientId(context: Context): String = EXAMPLE_CLIENT_ID
        }
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
        val experimentsList = listOf(createDefaultExperiment())
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
        val updateResult = ExperimentsSnapshot(listOf(createDefaultExperiment()), null)
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

        val storageSnapshot = ExperimentsSnapshot(
            listOf(createDefaultExperiment(id = "id1")),
            null
        )
        val updateResult = ExperimentsSnapshot(
            listOf(createDefaultExperiment(id = "id2")),
            null)
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
            createDefaultExperiment(id = "first-id"),
            createDefaultExperiment(id = "second-id")
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
        val experimentsList1 = listOf(createDefaultExperiment(id = "id"))
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
        val experimentsList2 = listOf(createDefaultExperiment(id = "id2"))
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
            createDefaultExperiment(
                id = "first-id",
                match = createDefaultMatcher(
                    deviceManufacturer = "manufacturer-1"
                )
            ),
            createDefaultExperiment(
                id = "second-id",
                match = createDefaultMatcher(
                    deviceManufacturer = "unknown",
                    appId = "test.appId"
                )
            ),
            createDefaultExperiment(
                id = "third-id",
                match = createDefaultMatcher(
                    deviceManufacturer = "unknown",
                    appDisplayVersion = "version.name"
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
                createDefaultExperiment(
                    id = "first-id",
                    match = createDefaultMatcher(
                        deviceManufacturer = "manufacturer-1"
                    )
                ),
                createDefaultExperiment(
                    id = "second-id",
                    match = createDefaultMatcher(
                        deviceManufacturer = "unknown",
                        appId = "test.appId"
                    )
                ),
                createDefaultExperiment(
                    id = "third-id",
                    match = createDefaultMatcher(
                        deviceManufacturer = "unknown",
                        appDisplayVersion = "version.name"
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
        assertTrue(experimentsMap["first-id"] == false)
        assertTrue(experimentsMap["second-id"] == true)
        assertTrue(experimentsMap["third-id"] == true)
    }

    @Test
    fun isInExperiment() {
        resetExperiments()
        var experimentsList = listOf(
            createDefaultExperiment(
                id = "first-id",
                match = createDefaultMatcher(
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
        packageInfo.versionName = "version.id"
        val packageManager = mock(PackageManager::class.java)
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        assertTrue(experiments.isInExperiment(context, "first-id"))

        // Update experiments with the experiment "first-id" not being active anymore.
        experimentsList = listOf(
            createDefaultExperiment(
                id = "first-id",
                match = createDefaultMatcher(
                    appId = "other.appId"
                )
            )
        )
        `when`(experimentSource.getExperiments(any())).thenReturn(ExperimentsSnapshot(experimentsList, null))
        experimentsUpdater.updateExperiments()

        assertFalse(experiments.isInExperiment(context, "first-id"))
        assertFalse(experiments.isInExperiment(context, "other-id"))
    }

    @Test
    fun withExperiment() {
        // Reset experiments with a mocked RNG for controlling branch choices.
        var branchDiceRoll = 1
        val valuesProvider = object : ValuesProvider() {
            override fun getRandomBranchValue(lower: Int, upper: Int): Int {
                return branchDiceRoll
            }
            override fun getClientId(context: Context): String = EXAMPLE_CLIENT_ID
        }
        resetExperiments(valuesProvider = valuesProvider)

        // Set up a mocked environment.
        val mockContext: Context = mock()
        `when`(mockContext.packageName).thenReturn("test.appId")
        val sharedPrefs: SharedPreferences = mock()
        val prefsEditor: SharedPreferences.Editor = mock()
        `when`(prefsEditor.putString(anyString(), anyString())).thenReturn(prefsEditor)
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(sharedPrefs.getBoolean(anyString(), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(mockContext.getSharedPreferences(anyString(), anyInt())).thenReturn(sharedPrefs)

        val packageInfo: PackageInfo = mock()
        packageInfo.versionName = "version.name"
        val packageManager: PackageManager = mock()
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(mockContext.packageManager).thenReturn(packageManager)

        // A helper to conveniently update the server-side experiments.
        val setServerExperiments = { experiments: List<Experiment> ->
            `when`(experimentSource.getExperiments(any())).thenReturn(ExperimentsSnapshot(experiments, null))
        }

        // First no experiment should be active.
        branchDiceRoll = 0
        setServerExperiments(emptyList())
        experiments.initialize(context, configuration)
        runBlocking {
            experimentsUpdater.testWaitForUpdate()
        }

        val invocations: MutableList<String> = java.util.ArrayList()
        experiments.withExperiment(mockContext, "first-id") {
            invocations.add(it)
        }
        assertTrue(invocations.isEmpty())
        assertNull(experiments.activeExperiment)

        // Get an initial experiments list from the servers.
        setServerExperiments(listOf(
            createDefaultExperiment(
                id = "first-id",
                match = createDefaultMatcher(
                    appId = "test.appId"
                ),
                branches = listOf(
                    Experiment.Branch("branch1", 1),
                    Experiment.Branch("branch2", 1),
                    Experiment.Branch("branch3", 1)
                )
            )
        ))

        // This should force choosing branch 2 of the experiment on update.
        branchDiceRoll = 1
        experimentsUpdater.updateExperiments()

        // Test active experiment.
        invocations.clear()
        experiments.withExperiment(mockContext, "first-id") {
            invocations.add(it)
        }
        assertEquals(listOf("branch2"), invocations)

        // Restart the library and disable updates. The same experiment and branch should still be
        // active from the cache.
        resetExperiments(valuesProvider = valuesProvider)
        branchDiceRoll = 0
        doAnswer {
            throw ExperimentDownloadException("test")
        }.`when`(experimentSource).getExperiments(any())
        experiments.initialize(context, configuration)
        runBlocking {
            experimentsUpdater.testWaitForUpdate()
        }

        experiments.withExperiment(mockContext, "first-id") {
            invocations.add(it)
        }
        assertEquals(listOf("branch2"), invocations)

        // Use an experiments list without the active experiment and trigger an update.
        // The experiments should deactivate.
        resetExperiments(valuesProvider = valuesProvider)
        setServerExperiments(listOf(
            createDefaultExperiment(
                id = "other-id",
                match = createDefaultMatcher(
                    appId = "other.appId"
                )
            )
        ))
        experiments.initialize(context, configuration)
        runBlocking {
            experimentsUpdater.testWaitForUpdate()
        }

        invocations.clear()
        experiments.withExperiment(mockContext, "first-id") {
            invocations.add(it)
        }
        assertTrue(invocations.isEmpty())
        assertNull(experiments.activeExperiment)

        // Change experiments list and trigger an update. The matcher now contains a non-matching
        // app id, so no experiments should activate.
        setServerExperiments(listOf(
            createDefaultExperiment(
                id = "first-id",
                match = createDefaultMatcher(
                    appId = "other.appId"
                )
            )
        ))
        experimentsUpdater.updateExperiments()
        assertNull(experiments.activeExperiment)

        invocations.clear()
        experiments.withExperiment(mockContext, "first-id") {
            invocations.add(it)
        }
        assertTrue(invocations.isEmpty())

        invocations.clear()
        experiments.withExperiment(mockContext, "other-id") {
            invocations.add(it)
        }
        assertTrue(invocations.isEmpty())
    }

    @Test
    fun getExperiment() {
        resetExperiments()
        val experimentsList = listOf(
            createDefaultExperiment(
                id = "first-id",
                match = createDefaultMatcher(
                    appId = "test.appId"
                )
            )
        )
        `when`(experimentSource.getExperiments(any())).thenReturn(ExperimentsSnapshot(experimentsList, null))
        experiments.initialize(context, configuration)
        runBlocking {
            experimentsUpdater.testWaitForUpdate()
        }

        assertEquals(experimentsList[0], experiments.getExperiment("first-id"))
        assertNull(experiments.getExperiment("other-id"))
    }

    private fun mockOverridePreferences(): Triple<Context, SharedPreferences.Editor, SharedPreferences.Editor> {
        val context: Context = mock()
        `when`(context.packageName).thenReturn("test.appId")
        val packageInfo: PackageInfo = mock()
        packageInfo.versionName = "version.name"
        val packageManager: PackageManager = mock()
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        // Mock experiment override prefs for enabled status.
        val sharedPrefsActive: SharedPreferences = mock()
        val prefsEditorEnabled: SharedPreferences.Editor = mock()
        `when`(prefsEditorEnabled.putBoolean(anyString(), anyBoolean())).thenReturn(prefsEditorEnabled)
        `when`(prefsEditorEnabled.remove(anyString())).thenReturn(prefsEditorEnabled)
        `when`(prefsEditorEnabled.clear()).thenReturn(prefsEditorEnabled)
        `when`(sharedPrefsActive.edit()).thenReturn(prefsEditorEnabled)
        `when`(sharedPrefsActive.getBoolean(anyString(), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(eq(ExperimentEvaluator.PREF_NAME_OVERRIDES_ENABLED), anyInt())).thenReturn(sharedPrefsActive)

        // Mock experiment override prefs for active branch.
        val sharedPrefsBranch: SharedPreferences = mock()
        val prefsEditorBranch: SharedPreferences.Editor = mock()
        `when`(prefsEditorBranch.putString(anyString(), anyString())).thenReturn(prefsEditorBranch)
        `when`(prefsEditorBranch.remove(anyString())).thenReturn(prefsEditorBranch)
        `when`(prefsEditorBranch.clear()).thenReturn(prefsEditorEnabled)
        `when`(sharedPrefsBranch.edit()).thenReturn(prefsEditorBranch)
        `when`(sharedPrefsBranch.getString(anyString(), eq(null))).thenAnswer { invocation -> invocation.arguments[1] as String }
        `when`(context.getSharedPreferences(eq(ExperimentEvaluator.PREF_NAME_OVERRIDES_BRANCH), anyInt())).thenReturn(sharedPrefsBranch)

        return Triple(context, prefsEditorEnabled, prefsEditorBranch)
    }

    @Test
    fun setOverride() {
        resetExperiments()

        val experimentsList = listOf(
            createDefaultExperiment(
                id = "first-id",
                match = createDefaultMatcher(
                    appId = "test.appId"
                )
            )
        )
        `when`(experimentSource.getExperiments(any())).thenReturn(ExperimentsSnapshot(experimentsList, null))
        experiments.initialize(context, configuration)
        runBlocking {
            experimentsUpdater.testWaitForUpdate()
        }

        val (
            context: Context,
            prefsEditorEnabled: SharedPreferences.Editor,
            prefsEditorBranch: SharedPreferences.Editor
        ) = mockOverridePreferences()

        assertTrue(experiments.isInExperiment(context, "first-id"))
        experiments.setOverride(context, "first-id", false, "branch-1")
        verify(prefsEditorEnabled).putBoolean("first-id", false)
        verify(prefsEditorBranch).putString("first-id", "branch-1")
        experiments.setOverride(context, "first-id", true, "branch-1")
        verify(prefsEditorEnabled).putBoolean("first-id", true)
        verify(prefsEditorBranch, times(2)).putString("first-id", "branch-1")

        runBlocking(Dispatchers.Default) {
            assertTrue(experiments.isInExperiment(context, "first-id"))
            experiments.setOverrideNow(context, "first-id", false, "branch-2")
            verify(prefsEditorEnabled, times(2)).putBoolean("first-id", false)
            verify(prefsEditorBranch, times(1)).putString("first-id", "branch-2")
            experiments.setOverrideNow(context, "first-id", true, "branch-2")
            verify(prefsEditorEnabled, times(2)).putBoolean("first-id", true)
            verify(prefsEditorBranch, times(2)).putString("first-id", "branch-2")
        }
    }

    @Test
    fun clearOverride() {
        resetExperiments()
        val experimentsList = listOf(
            createDefaultExperiment(
                id = "first-id",
                match = createDefaultMatcher(
                    appId = "test.appId"
                )
            )
        )
        `when`(experimentSource.getExperiments(any())).thenReturn(ExperimentsSnapshot(experimentsList, null))
        experiments.initialize(context, configuration)
        runBlocking {
            experimentsUpdater.testWaitForUpdate()
        }

        val (
            context: Context,
            prefsEditorEnabled: SharedPreferences.Editor,
            prefsEditorBranch: SharedPreferences.Editor
        ) = mockOverridePreferences()

        assertTrue(experiments.isInExperiment(context, "first-id"))
        experiments.setOverride(context, "first-id", false, "branch-1")
        verify(prefsEditorEnabled).putBoolean("first-id", false)
        verify(prefsEditorBranch).putString("first-id", "branch-1")
        experiments.clearOverride(context, "first-id")
        verify(prefsEditorEnabled).remove("first-id")
        verify(prefsEditorBranch).remove("first-id")

        runBlocking(Dispatchers.Default) {
            experiments.setOverrideNow(context, "first-id", false, "branch-1")
            experiments.clearOverrideNow(context, "first-id")
            verify(prefsEditorEnabled, times(2)).remove("first-id")
            verify(prefsEditorBranch, times(2)).remove("first-id")
        }
    }

    @Test
    fun clearAllOverrides() {
        resetExperiments()
        val experimentsList = listOf(
            createDefaultExperiment(
                id = "first-id",
                match = createDefaultMatcher(
                    appId = "test.appId"
                )
            )
        )
        `when`(experimentSource.getExperiments(any())).thenReturn(ExperimentsSnapshot(experimentsList, null))
        experiments.initialize(context, configuration)
        runBlocking {
            experimentsUpdater.testWaitForUpdate()
        }

        val (
            context: Context,
            prefsEditorEnabled: SharedPreferences.Editor,
            _: SharedPreferences.Editor
        ) = mockOverridePreferences()

        val packageInfo: PackageInfo = mock()
        packageInfo.versionName = "version.name"
        val packageManager: PackageManager = mock()
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        assertTrue(experiments.isInExperiment(context, "first-id"))
        experiments.setOverride(context, "first-id", false, "branch-2")
        experiments.clearAllOverrides(context)
        verify(prefsEditorEnabled).clear()

        runBlocking(Dispatchers.Default) {
            experiments.setOverrideNow(context, "first-id", false, "branch-2")
            experiments.clearAllOverridesNow(context)
            verify(prefsEditorEnabled, times(2)).clear()
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

        resetExperiments(valuesProvider = ValuesProvider())
        experiments.initialize(context, configuration)
        runBlocking {
            experimentsUpdater.testWaitForUpdate()
        }
        assertTrue(experiments.getUserBucket(mockContext) == 54)
    }

    @Test
    fun `getUserBucket() with overridden clientId`() {
        val source = getDefaultExperimentSourceMock()
        val storage = getDefaultExperimentStorageMock()

        resetExperiments(source, storage, object : ValuesProvider() {
            override fun getClientId(context: Context): String = EXAMPLE_CLIENT_ID
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