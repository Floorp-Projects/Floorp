/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

import android.content.Context
import android.content.SharedPreferences
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.work.testing.WorkManagerTestInitHelper
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.runBlocking
import mozilla.components.service.glean.Glean
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.Assert.assertNotNull
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.ArgumentMatchers.anyString
import org.mockito.ArgumentMatchers.isNull
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doAnswer
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import java.io.File

@RunWith(AndroidJUnit4::class)
class ExperimentsTest {
    private var context: Context = ApplicationProvider.getApplicationContext()
    private var mockContext: Context = getDefaultMockedContext()
    private lateinit var configuration: Configuration
    private lateinit var experiments: ExperimentsInternalAPI
    private lateinit var experimentStorage: FlatFileExperimentStorage
    private lateinit var experimentSource: KintoExperimentSource
    private lateinit var experimentsUpdater: ExperimentsUpdater

    private val EXAMPLE_CLIENT_ID = "c641eacf-c30c-4171-b403-f077724e848a"

    private val experimentsList = listOf(
        createDefaultExperiment(
            id = "first-id",
            match = createDefaultMatcher(
                deviceManufacturer = "manufacturer-1"
            )
        ),
        createDefaultExperiment(
            id = "second-id",
            match = createDefaultMatcher(
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

    private fun getDefaultMockedContext(): Context {
        val context: Context = mock()
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPrefs: SharedPreferences = mock()
        val prefsEditor: SharedPreferences.Editor = mock()
        `when`(prefsEditor.putString(anyString(), anyString())).thenReturn(prefsEditor)
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(sharedPrefs.getBoolean(anyString(), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(prefsEditor.clear()).thenReturn(prefsEditor)
        `when`(context.getSharedPreferences(anyString(), anyInt())).thenReturn(sharedPrefs)

        val packageInfo: PackageInfo = mock()
        packageInfo.versionName = "version.name"
        val packageManager: PackageManager = mock()
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        return context
    }

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
            override fun getVersion(context: Context): String = "1.0"
        }
    ) {
        // Initialize WorkManager (early) for instrumentation tests.
        WorkManagerTestInitHelper.initializeTestWorkManager(context)

        // Setting the endpoint to a non-existent one to prevent actual experiments from being
        // downloaded to tests.
        configuration = Configuration(kintoEndpoint = "https://example.invalid")
        experiments = spy(ExperimentsInternalAPI())
        experiments.valuesProvider = valuesProvider

        experimentStorage = storage
        `when`(experiments.getExperimentsStorage(context)).thenReturn(storage)
        `when`(experimentStorage.save(any())).then {
            // Do nothing
        }

        experimentsUpdater = spy(ExperimentsUpdater(context, experiments))
        `when`(experiments.getExperimentsUpdater(context)).thenReturn(experimentsUpdater)
        experimentSource = source
        `when`(experimentsUpdater.getExperimentSource(configuration)).thenReturn(experimentSource)

        // Since WorkManager doesn't support working with robolectric, we need to mock running the
        // task with a way to execute synchronously to avoid a lot of headaches. For some test cases
        // we also need to support running from a separate thread, so we use runBlocking and a
        // coroutine to attempt to await the background thread.
        `when`(experimentsUpdater.scheduleUpdates()).then {
            experimentsUpdater.updateExperiments()
        }
    }

    @After
    fun cleanup() {
        // Make sure there isn't a persisted file.  When using the mockContext, a .json file is
        // written to the current directory when the flat file persisted storage is written and
        // can show up as a new file in Git.
        experiments.getExperimentsStorage(mockContext).delete()
    }

    @Test
    fun `initialize experiments`() {
        // Run this test with experiment updates as async to help validate async library operations
        resetExperiments()

        experiments.initialize(context, configuration)

        verify(experimentStorage).retrieve()
        verify(experimentsUpdater).initialize(any())
        verify(experimentSource).getExperiments(any())
    }

    @Test
    fun `updating of experiments on library init`() {
        // Run this test with experiment updates as async to help validate async library operations
        resetExperiments()

        // Set up the Kinto-side experiments.
        val experimentsList = listOf(createDefaultExperiment())
        val snapshot = ExperimentsSnapshot(experimentsList, null)
        `when`(experimentSource.getExperiments(any())).thenReturn(snapshot)

        experiments.initialize(context, configuration)

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
        experiments.initialize(mockContext, configuration)
        experiments.experimentsResult = ExperimentsSnapshot(experimentsList, null)
        experiments.findAndStartActiveExperiment()

        assertNotNull(experiments.activeExperiment)
        assertEquals("second-id", experiments.activeExperiment!!.experiment.id)
    }

    @Test
    fun `enrollment and unenrollment recorded in Glean`() {
        resetExperiments()
        experiments.initialize(mockContext, configuration)

        // Update the experiments with an empty list to unenroll from any experiments
        experiments.onExperimentsUpdated(ExperimentsSnapshot(listOf(), null))

        // Make sure that we aren't already enrolled
        assertFalse(Glean.testIsExperimentActive("second-id"))

        // Simulate update from server that should enroll us in 'second-id' experiment
        experiments.onExperimentsUpdated(ExperimentsSnapshot(experimentsList, null))
        assertNotNull(experiments.activeExperiment)
        assertEquals("second-id", experiments.activeExperiment!!.experiment.id)

        // Verify that we are now enrolled in the experiment
        assertTrue(Glean.testIsExperimentActive("second-id"))

        // Update the experiments with an empty list to unenroll from any experiments
        experiments.onExperimentsUpdated(ExperimentsSnapshot(listOf(), null))

        // Verify we are now unenrolled
        assertFalse(Glean.testIsExperimentActive("second-id"))
    }

    @Test
    fun isInExperiment() {

        run {
            resetExperiments()

            val experimentsList = listOf(
                createDefaultExperiment(
                    id = "first-id",
                    match = createDefaultMatcher(
                        appId = "test.appId"
                    )
                )
            )

            experiments.initialize(mockContext, configuration)
            experiments.experimentsResult = ExperimentsSnapshot(experimentsList, null)
            experiments.findAndStartActiveExperiment()

            assertTrue(experiments.isInExperiment("first-id", "only-branch"))
        }

        run {
            resetExperiments()

            // Update experiments with the experiment "first-id" not being active anymore.
            val experimentsList = listOf(
                createDefaultExperiment(
                    id = "first-id",
                    match = createDefaultMatcher(
                        appId = "other.appId"
                    )
                )
            )

            experiments.initialize(mockContext, configuration)
            experiments.experimentsResult = ExperimentsSnapshot(experimentsList, null)
            experiments.findAndStartActiveExperiment()

            assertFalse(experiments.isInExperiment("first-id", "only-branch"))
            assertFalse(experiments.isInExperiment("other-id", "only-branch"))
        }
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

        val sharedPrefs = mock(SharedPreferences::class.java)
        val prefsEditor = mock(SharedPreferences.Editor::class.java)

        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(prefsEditor.putBoolean(anyString(), anyBoolean())).thenReturn(prefsEditor)
        `when`(prefsEditor.putString(anyString(), anyString())).thenReturn(prefsEditor)
        `when`(prefsEditor.clear()).thenReturn(prefsEditor)
        `when`(mockContext.getSharedPreferences(anyString(), anyInt())).thenReturn(sharedPrefs)
        `when`(sharedPrefs.getString(eq("id"), isNull())).thenReturn("first-id")
        `when`(sharedPrefs.getString(eq("branch"), isNull())).thenReturn("branch2")
        `when`(mockContext.packageName).thenReturn("test.appId")

        // First no experiment should be active.
        branchDiceRoll = 0
        `when`(experimentSource.getExperiments(any())).thenReturn(ExperimentsSnapshot(emptyList(), null))
        experiments.initialize(mockContext, configuration)

        val invocations: MutableList<String> = java.util.ArrayList()
        experiments.withExperiment("first-id") {
            invocations.add(it)
        }
        assertNull(experiments.activeExperiment)

        // Get an initial experiments list from the servers.
        var experimentsList = listOf(
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
        )
        // This should force choosing branch 2 of the experiment on update.
        branchDiceRoll = 1
        experiments.onExperimentsUpdated(ExperimentsSnapshot(experimentsList, null))

        // Test active experiment.
        invocations.clear()
        experiments.withExperiment("first-id") {
            invocations.add(it)
        }
        assertNotNull(experiments.activeExperiment)
        assertEquals(listOf("branch2"), invocations)

        // Restart the library and disable updates. The same experiment and branch should still be
        // active from the cache.
        invocations.clear()
        resetExperiments(valuesProvider = valuesProvider)
        branchDiceRoll = 0
        experiments.updater = experimentsUpdater
        experiments.updater.source = experimentSource
        doAnswer {
            throw ExperimentDownloadException("test")
        }.`when`(experimentSource).getExperiments(any())
        experiments.initialize(mockContext, configuration)

        experiments.withExperiment("first-id") {
            invocations.add(it)
        }
        assertEquals(listOf("branch2"), invocations)

        // Use an experiments list without the active experiment and trigger an update.
        // The experiments should deactivate.
        experimentsList = listOf(
            createDefaultExperiment(
                id = "other-id",
                match = createDefaultMatcher(
                    appId = "other.appId"
                )
            )
        )
        experiments.onExperimentsUpdated(ExperimentsSnapshot(experimentsList, null))

        invocations.clear()
        experiments.withExperiment("first-id") {
            invocations.add(it)
        }
        assertTrue(invocations.isEmpty())
        assertNull(experiments.activeExperiment)

        // Change experiments list and trigger an update. The matcher now contains a non-matching
        // app id, so no experiments should activate.
        experimentsList = listOf(
            createDefaultExperiment(
                id = "first-id",
                match = createDefaultMatcher(
                    appId = "other.appId"
                )
            )
        )
        experiments.onExperimentsUpdated(ExperimentsSnapshot(experimentsList, null))
        assertNull(experiments.activeExperiment)

        invocations.clear()
        experiments.withExperiment("first-id") {
            invocations.add(it)
        }
        assertTrue(invocations.isEmpty())

        invocations.clear()
        experiments.withExperiment("other-id") {
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
                id = "first-id"
            )
        )
        `when`(experimentSource.getExperiments(any())).thenReturn(ExperimentsSnapshot(experimentsList, null))
        experiments.initialize(context, configuration)

        val (
            context: Context,
            prefsEditorEnabled: SharedPreferences.Editor,
            prefsEditorBranch: SharedPreferences.Editor
        ) = mockOverridePreferences()

        assertTrue(experiments.isInExperiment("first-id", "only-branch"))
        experiments.setOverride(context, "first-id", false, "branch-1")
        verify(prefsEditorEnabled).putBoolean("first-id", false)
        verify(prefsEditorBranch).putString("first-id", "branch-1")
        experiments.setOverride(context, "first-id", true, "branch-1")
        verify(prefsEditorEnabled).putBoolean("first-id", true)
        verify(prefsEditorBranch, times(2)).putString("first-id", "branch-1")

        runBlocking(Dispatchers.Default) {
            assertTrue(experiments.isInExperiment("first-id", "only-branch"))
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
                id = "first-id"
            )
        )
        `when`(experimentSource.getExperiments(any())).thenReturn(ExperimentsSnapshot(experimentsList, null))
        experiments.initialize(context, configuration)

        val (
            context: Context,
            prefsEditorEnabled: SharedPreferences.Editor,
            prefsEditorBranch: SharedPreferences.Editor
        ) = mockOverridePreferences()

        assertTrue(experiments.isInExperiment("first-id", "only-branch"))
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
                id = "first-id"
            )
        )
        `when`(experimentSource.getExperiments(any())).thenReturn(ExperimentsSnapshot(experimentsList, null))
        experiments.initialize(context, configuration)

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

        assertTrue(experiments.isInExperiment("first-id", "only-branch"))
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
        assertEquals(954, experiments.getUserBucket(mockContext))
    }

    @Test
    fun `getUserBucket() with overridden clientId`() {
        val source = getDefaultExperimentSourceMock()
        val storage = getDefaultExperimentStorageMock()

        resetExperiments(source, storage, object : ValuesProvider() {
            override fun getClientId(context: Context): String = EXAMPLE_CLIENT_ID
        })
        experiments.initialize(context, configuration)

        assertEquals(779, experiments.getUserBucket(context))

        resetExperiments(source, storage, object : ValuesProvider() {
            override fun getClientId(context: Context): String = "01a15650-9a5d-4383-a7ba-2f047b25c620"
        })
        experiments.initialize(context, configuration)

        assertEquals(355, experiments.getUserBucket(context))
    }

    @Test
    fun `loading corrupt JSON`() {
        val experimentSource: KintoExperimentSource = mock()

        val file = File(ApplicationProvider.getApplicationContext<Context>().filesDir,
            "corrupt-experiments.json")
        file.writer().use {
            it.write("""{"experiment":[""")
        }

        val experimentStorage = FlatFileExperimentStorage(file)

        // resetExperiments causes an error here due to how we have mocked things with the updater
        // which ends up calling onExperimentsUpdated with a null parameter causing the error.  In
        // order to get around this, we need to minimize the mocking of the updater to avoid the
        // error.
        WorkManagerTestInitHelper.initializeTestWorkManager(context)
        // Setting the endpoint to a non-existent one to prevent actual experiments from being
        // downloaded to tests.
        configuration = Configuration(kintoEndpoint = "https://example.invalid")
        experiments = spy(ExperimentsInternalAPI())
        experiments.valuesProvider = ValuesProvider()

        `when`(experiments.getExperimentsStorage(context)).thenReturn(experimentStorage)

        experimentsUpdater = spy(ExperimentsUpdater(context, experiments))
        `when`(experiments.getExperimentsUpdater(context)).thenReturn(experimentsUpdater)
        `when`(experimentsUpdater.getExperimentSource(configuration)).thenReturn(experimentSource)

        experiments.initialize(context, configuration)
        experiments.loadExperiments() // Should not throw
    }
}
