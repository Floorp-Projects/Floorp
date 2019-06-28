/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

import android.content.Context
import android.content.SharedPreferences
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.ArgumentMatchers.anyString
import org.mockito.ArgumentMatchers.eq
import org.mockito.Mockito.`when`
import org.mockito.Mockito.verify
import kotlin.reflect.full.functions
import kotlin.reflect.jvm.isAccessible

@RunWith(AndroidJUnit4::class)
class ExperimentEvaluatorTest {
    private lateinit var context: Context
    private lateinit var sharedPrefsOverrideEnabled: SharedPreferences
    private lateinit var sharedPrefsOverrideBranch: SharedPreferences
    private lateinit var enabledEditor: SharedPreferences.Editor
    private lateinit var branchEditor: SharedPreferences.Editor
    private lateinit var packageManager: PackageManager
    private lateinit var packageInfo: PackageInfo
    private var currentTime = System.currentTimeMillis() / 1000

    private fun testReset(appId: String = "test.appId", versionName: String = "test.version") {
        context = mock()
        sharedPrefsOverrideEnabled = mock()
        sharedPrefsOverrideBranch = mock()
        packageManager = mock()
        packageInfo = PackageInfo()

        `when`(context.packageName).thenReturn(appId)

        packageInfo.versionName = versionName
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val prefEnabled = ExperimentEvaluator.PREF_NAME_OVERRIDES_ENABLED
        val prefBranch = ExperimentEvaluator.PREF_NAME_OVERRIDES_BRANCH

        `when`(context.getSharedPreferences(eq(prefEnabled), eq(Context.MODE_PRIVATE))).thenReturn(sharedPrefsOverrideEnabled)
        `when`(sharedPrefsOverrideEnabled.getBoolean(anyString(), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        enabledEditor = mock()
        `when`(enabledEditor.putBoolean(anyString(), anyBoolean())).thenReturn(enabledEditor)
        `when`(enabledEditor.remove(anyString())).thenReturn(enabledEditor)
        `when`(enabledEditor.clear()).thenReturn(enabledEditor)
        `when`(sharedPrefsOverrideEnabled.edit()).thenReturn(enabledEditor)

        `when`(context.getSharedPreferences(eq(prefBranch), eq(Context.MODE_PRIVATE))).thenReturn(sharedPrefsOverrideBranch)
        `when`(sharedPrefsOverrideBranch.getString(anyString(), anyString())).thenAnswer { invocation -> invocation.arguments[1] as String }
        branchEditor = mock()
        `when`(branchEditor.putString(anyString(), anyString())).thenReturn(branchEditor)
        `when`(branchEditor.remove(anyString())).thenReturn(branchEditor)
        `when`(branchEditor.clear()).thenReturn(branchEditor)
        `when`(sharedPrefsOverrideBranch.edit()).thenReturn(branchEditor)
    }

    @Suppress("SameParameterValue")
    private fun setOverride(experimentName: String, isActive: Boolean, branchName: String) {
        `when`(sharedPrefsOverrideEnabled.getBoolean(eq(experimentName), anyBoolean())).thenReturn(isActive)
        `when`(sharedPrefsOverrideBranch.getString(eq(experimentName), eq(null))).thenReturn(branchName)
        `when`(sharedPrefsOverrideEnabled.contains(eq(experimentName))).thenReturn(true)
        `when`(sharedPrefsOverrideBranch.contains(eq(experimentName))).thenReturn(true)
    }

    @Test
    fun `evaluate empty matchers`() {
        val experiment = createDefaultExperiment(
            id = "testexperiment",
            lastModified = currentTime,
            buckets = Experiment.Buckets(20, 70),
            match = createDefaultMatcher()
        )

        testReset(appId = "other.appId")

        val evaluator = ExperimentEvaluator()
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))
    }

    @Test
    fun `evaluate buckets`() {
        testReset(appId = "test.appId", versionName = "test.version")

        val experiment = createDefaultExperiment(
            id = "testexperiment",
            match = createDefaultMatcher(
                appId = "test.appId",
                localeLanguage = "eng",
                regions = listOf("USA", "GBR"),
                appDisplayVersion = "test.version",
                deviceManufacturer = "unknown",
                deviceModel = "robolectric",
                localeCountry = "USA"
            ),
            buckets = Experiment.Buckets(20, 50),
            lastModified = currentTime
        )

        val evaluator = ExperimentEvaluator()
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 21))
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 69))
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 19))
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 70))
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 71))
    }

    @Test
    fun `evaluate appId`() {
        testReset(appId = "other.appId", versionName = "test.version")

        val experiment = createDefaultExperiment(
            id = "testexperiment",
            match = createDefaultMatcher(
                localeLanguage = "eng",
                appId = "^test$",
                regions = listOf("USA", "GBR"),
                appDisplayVersion = "test.version",
                deviceManufacturer = "unknown",
                deviceModel = "robolectric",
                localeCountry = "USA"
            ),
            buckets = Experiment.Buckets(20, 70),
            lastModified = currentTime
        )

        val evaluator = ExperimentEvaluator()
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))
        `when`(context.packageName).thenReturn("test")
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))
    }

    @Test
    fun `evaluate localeLanguage`() {
        testReset(appId = "test.appId", versionName = "test.version")

        var experiment = createDefaultExperiment(
            id = "testexperiment",
            match = createDefaultMatcher(
                localeLanguage = "eng",
                appId = "test.appId",
                regions = listOf("USA", "GBR"),
                appDisplayVersion = "test.version",
                deviceManufacturer = "unknown",
                deviceModel = "robolectric",
                localeCountry = "USA"
            ),
            buckets = Experiment.Buckets(20, 70),
            lastModified = currentTime
        )

        val evaluator = ExperimentEvaluator()
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        experiment = createDefaultExperiment(
            id = "testexperiment",
            match = createDefaultMatcher(
                localeLanguage = "esp",
                appId = "test.appId",
                regions = listOf("USA", "GBR"),
                appDisplayVersion = "test.version",
                deviceManufacturer = "unknown",
                deviceModel = "robolectric",
                localeCountry = "USA"
            ),
            buckets = Experiment.Buckets(20, 70),
            lastModified = currentTime
        )
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))
    }

    @Test
    fun `evaluate localeCountry`() {
        testReset(appId = "test.appId", versionName = "test.version")

        var experiment = createDefaultExperiment(
            id = "testexperiment",
            match = createDefaultMatcher(
                localeLanguage = "eng",
                appId = "test.appId",
                regions = listOf("USA", "GBR"),
                appDisplayVersion = "test.version",
                deviceManufacturer = "unknown",
                deviceModel = "robolectric",
                localeCountry = "USA"
            ),
            buckets = Experiment.Buckets(20, 70),
            lastModified = currentTime
        )

        val evaluator = ExperimentEvaluator()
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        experiment = createDefaultExperiment(
            id = "testexperiment",
            match = createDefaultMatcher(
                localeLanguage = "eng",
                appId = "test.appId",
                regions = listOf("USA", "GBR"),
                appDisplayVersion = "test.version",
                deviceManufacturer = "unknown",
                deviceModel = "robolectric",
                localeCountry = "ESP"
            ),
            buckets = Experiment.Buckets(20, 70),
            lastModified = currentTime
        )

        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))
    }

    @Test
    fun `evaluate appDisplayVersion`() {
        testReset(appId = "test.appId", versionName = "test.version")

        val experiment = createDefaultExperiment(
            id = "testexperiment",
            match = createDefaultMatcher(
                localeLanguage = "eng",
                appId = "test.appId",
                regions = listOf("USA", "GBR"),
                appDisplayVersion = "test.version",
                deviceManufacturer = "unknown",
                deviceModel = "robolectric",
                localeCountry = "USA"
            ),
            buckets = Experiment.Buckets(20, 70),
            lastModified = currentTime
        )

        val evaluator = ExperimentEvaluator()
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        packageInfo.versionName = "other.version"
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))
    }

    @Test
    fun `evaluate appMinVersion`() {
        testReset(appId = "test.appId", versionName = "1.0.0")

        val experiment = createDefaultExperiment(
            id = "testexperiment",
            match = createDefaultMatcher(
                localeLanguage = "eng",
                appId = "test.appId",
                regions = listOf("USA", "GBR"),
                appMinVersion = "1.0.0",
                deviceManufacturer = "unknown",
                deviceModel = "robolectric",
                localeCountry = "USA"
            ),
            buckets = Experiment.Buckets(20, 70),
            lastModified = currentTime
        )

        val evaluator = ExperimentEvaluator()
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        packageInfo.versionName = "1"
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        packageInfo.versionName = "1.0"
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        packageInfo.versionName = "1.0.1"
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        packageInfo.versionName = "1.1.18"
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        packageInfo.versionName = "1.1.18.43123"
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        packageInfo.versionName = "1.1.18.34.2345"
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        packageInfo.versionName = "0.1.18"
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        packageInfo.versionName = "0.9.9"
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))
    }

    @Test
    fun `evaluate appMaxVersion`() {
        testReset(appId = "test.appId", versionName = "2.0.0")

        val experiment = createDefaultExperiment(
            id = "testexperiment",
            match = createDefaultMatcher(
                localeLanguage = "eng",
                appId = "test.appId",
                regions = listOf("USA", "GBR"),
                appMaxVersion = "2.0.0",
                deviceManufacturer = "unknown",
                deviceModel = "robolectric",
                localeCountry = "USA"
            ),
            buckets = Experiment.Buckets(20, 70),
            lastModified = currentTime
        )

        val evaluator = ExperimentEvaluator()
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        packageInfo.versionName = "1"
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        packageInfo.versionName = "1.9"
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        packageInfo.versionName = "1.9.9999"
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        packageInfo.versionName = "1.1.18"
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        packageInfo.versionName = "2.0.1"
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        packageInfo.versionName = "2.0.1.2.3"
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        packageInfo.versionName = "67.1.12345678"
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))
    }

    @Test
    fun `evaluate a min and max version together`() {
        testReset(appId = "test.appId")

        val experiment = createDefaultExperiment(
            id = "testexperiment",
            match = createDefaultMatcher(
                localeLanguage = "eng",
                appId = "test.appId",
                regions = listOf("USA", "GBR"),
                appMinVersion = "1.0.0",
                appMaxVersion = "2.0.0",
                deviceManufacturer = "unknown",
                deviceModel = "robolectric",
                localeCountry = "USA"
            ),
            buckets = Experiment.Buckets(20, 70),
            lastModified = currentTime
        )

        val evaluator = ExperimentEvaluator()

        packageInfo.versionName = "0.1"
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        packageInfo.versionName = "0.0.1"
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        packageInfo.versionName = "0.9.9"
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        packageInfo.versionName = "0.9.9.9.9.9"
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        packageInfo.versionName = "1.0.0"
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        packageInfo.versionName = "1.0.1"
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        packageInfo.versionName = "1.9.9999"
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        packageInfo.versionName = "1.9.9999.999.99"
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        packageInfo.versionName = "2.0.1"
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        packageInfo.versionName = "2.1.18"
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))
    }

    @Test
    fun `evaluate deviceModel`() {
        testReset(appId = "test.appId", versionName = "test.version")

        var experiment = createDefaultExperiment(
            id = "testexperiment",
            match = createDefaultMatcher(
                localeLanguage = "eng",
                appId = "test.appId",
                regions = listOf("USA", "GBR"),
                appDisplayVersion = "test.version",
                deviceManufacturer = "unknown",
                deviceModel = "robolectric",
                localeCountry = "USA"
            ),
            buckets = Experiment.Buckets(20, 70),
            lastModified = currentTime
        )

        val evaluator = ExperimentEvaluator()
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        experiment = createDefaultExperiment(
            id = "testexperiment",
            match = createDefaultMatcher(
                localeLanguage = "eng",
                appId = "test.appId",
                regions = listOf("USA", "GBR"),
                appDisplayVersion = "test.version",
                deviceManufacturer = "unknown",
                deviceModel = "otherdevice",
                localeCountry = "USA"
            ),
            buckets = Experiment.Buckets(20, 70),
            lastModified = currentTime
        )

        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))
    }

    @Test
    fun `evaluate deviceManufacturer`() {
        testReset(appId = "test.appId", versionName = "test.version")

        var experiment = createDefaultExperiment(
            id = "testexperiment",
            match = createDefaultMatcher(
                localeLanguage = "eng",
                appId = "test.appId",
                regions = listOf("USA", "GBR"),
                appDisplayVersion = "test.version",
                deviceManufacturer = "unknown",
                deviceModel = "robolectric",
                localeCountry = "USA"
            ),
            buckets = Experiment.Buckets(20, 70),
            lastModified = currentTime
        )

        val evaluator = ExperimentEvaluator()
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        experiment = createDefaultExperiment(
            id = "testexperiment",
            match = createDefaultMatcher(
                localeLanguage = "eng",
                appId = "test.appId",
                regions = listOf("USA", "GBR"),
                appDisplayVersion = "test.version",
                deviceManufacturer = "other",
                deviceModel = "robolectric",
                localeCountry = "USA"
            ),
            buckets = Experiment.Buckets(20, 70),
            lastModified = currentTime
        )
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))
    }

    @Test
    fun `evaluate region`() {
        testReset(appId = "test.appId", versionName = "test.version")

        val experiment = createDefaultExperiment(
            id = "testexperiment",
            match = createDefaultMatcher(
                localeLanguage = "eng",
                appId = "test.appId",
                regions = listOf("USA", "GBR"),
                appDisplayVersion = "test.version",
                deviceManufacturer = "unknown",
                deviceModel = "robolectric",
                localeCountry = "USA"
            ),
            buckets = Experiment.Buckets(20, 70),
            lastModified = currentTime
        )

        var evaluator = ExperimentEvaluator(object : ValuesProvider() {
            override fun getRegion(context: Context): String? {
                return "USA"
            }
        })

        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        evaluator = ExperimentEvaluator(object : ValuesProvider() {
            override fun getRegion(context: Context): String? {
                return "ESP"
            }
        })

        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))
    }

    @Test
    fun `override an inactive experiment to active`() {
        testReset(appId = "test.appId", versionName = "test.version")

        // Use another app id, so this experiment doesn't activate.
        val experiment = createDefaultExperiment(
            id = "some-experiment",
            match = createDefaultMatcher(appId = "other.appId")
        )
        val evaluator = ExperimentEvaluator()
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("some-experiment"), listOf(experiment), 0))

        // Now activate the override. The experiment should then evaluate to active.
        setOverride("some-experiment", true, "branch-3")
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("some-experiment"), listOf(experiment), 0))
    }

    @Test
    fun `override an active experiment to inactive`() {
        testReset(appId = "test.appId", versionName = "test.version")

        // This experiment should activate.
        val experiment = createDefaultExperiment(
            id = "some-experiment"
        )
        val evaluator = ExperimentEvaluator()
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("some-experiment"), listOf(experiment), 50))

        // Now set an override for it. The experiment should not evaluate to active anymore.
        setOverride("some-experiment", false, "branch-3")
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("some-experiment"), listOf(experiment), 50))
    }

    @Test
    fun `evaluate no experiment matches descriptor`() {
        testReset(appId = "test.appId", versionName = "test.version")

        val savedExperiment = createDefaultExperiment(id = "wrongid")
        val descriptor = ExperimentDescriptor("testname")
        assertNull(ExperimentEvaluator().evaluate(context, descriptor, listOf(savedExperiment), 20))
    }

    @Test
    fun `verify setOverride() activate writes to SharedPreferences`() {
        testReset()

        val evaluator = ExperimentEvaluator()
        evaluator.setOverride(context, ExperimentDescriptor("exp-id"), true, "branch-1")
        verify(enabledEditor).putBoolean("exp-id", true)
        verify(branchEditor).putString("exp-id", "branch-1")
    }

    @Test
    fun `verify setOverride() deactivate writes to SharedPreferences`() {
        testReset()

        val evaluator = ExperimentEvaluator()
        evaluator.setOverride(context, ExperimentDescriptor("exp-2-id"), false, "branch-2")
        verify(enabledEditor).putBoolean("exp-2-id", false)
        verify(branchEditor).putString("exp-2-id", "branch-2")
    }

    @Test
    fun `verify clearOverride() writes to SharedPreferences`() {
        testReset()

        val evaluator = ExperimentEvaluator()
        evaluator.clearOverride(context, ExperimentDescriptor("exp-id"))
        verify(enabledEditor).remove("exp-id")
        verify(branchEditor).remove("exp-id")
    }

    @Test
    fun `verify clearAllOverrides() writes to SharedPreferences`() {
        testReset()

        val evaluator = ExperimentEvaluator()
        evaluator.clearAllOverrides(context)
        verify(enabledEditor).clear()
        verify(branchEditor).clear()
    }

    @Test
    fun `test overriding client id`() {
        val evaluator1 = ExperimentEvaluator(object : ValuesProvider() {
            override fun getClientId(context: Context): String = "c641eacf-c30c-4171-b403-f077724e848a"
        })

        assertEquals(779, evaluator1.getUserBucket(testContext))

        val evaluator2 = ExperimentEvaluator(object : ValuesProvider() {
            override fun getClientId(context: Context): String = "01a15650-9a5d-4383-a7ba-2f047b25c620"
        })

        assertEquals(355, evaluator2.getUserBucket(testContext))
    }

    @Test
    fun `test even distribution`() {
        testReset()

        val sharedPrefs: SharedPreferences = mock()
        val prefsEditor: SharedPreferences.Editor = mock()
        `when`(sharedPrefs.edit()).thenReturn(prefsEditor)
        `when`(prefsEditor.putBoolean(anyString(), anyBoolean())).thenReturn(prefsEditor)
        `when`(prefsEditor.putString(anyString(), anyString())).thenReturn(prefsEditor)
        `when`(context.getSharedPreferences(anyString(), anyInt())).thenReturn(sharedPrefs)

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
                Assert.assertTrue(it.value in 0..9)
            }

        distribution
            .groupingBy { it / 10 }
            .eachCount()
            .forEach {
                Assert.assertTrue(it.value in 0..29)
            }

        distribution
            .groupingBy { it / 100 }
            .eachCount()
            .forEach {
                Assert.assertTrue(it.value in 50..150)
            }

        distribution
            .groupingBy { it / 500 }
            .eachCount()
            .forEach {
                Assert.assertTrue(it.value in 350..650)
            }
    }

    @Test
    fun `test selectBranchByWeight()`() {
        val e = ExperimentEvaluator()

        var branches = listOf(
            Experiment.Branch("1", 2)
        )

        assertEquals("1", e.selectBranchByWeight(0, branches).name)
        assertEquals("1", e.selectBranchByWeight(1, branches).name)

        branches = listOf(
            Experiment.Branch("1", 1),
            Experiment.Branch("2", 2)
        )

        assertEquals("1", e.selectBranchByWeight(0, branches).name)
        assertEquals("2", e.selectBranchByWeight(1, branches).name)
        assertEquals("2", e.selectBranchByWeight(2, branches).name)

        branches = listOf(
            Experiment.Branch("1", 2),
            Experiment.Branch("2", 4),
            Experiment.Branch("3", 1)
        )

        assertEquals("1", e.selectBranchByWeight(0, branches).name)
        assertEquals("1", e.selectBranchByWeight(1, branches).name)
        assertEquals("2", e.selectBranchByWeight(2, branches).name)
        assertEquals("2", e.selectBranchByWeight(3, branches).name)
        assertEquals("2", e.selectBranchByWeight(4, branches).name)
        assertEquals("2", e.selectBranchByWeight(5, branches).name)
        assertEquals("3", e.selectBranchByWeight(6, branches).name)
    }

    @Test(expected = AssertionError::class)
    fun `test selectBranchByWeight() asserts upper out-of-bounds`() {
        val branches = listOf(
            Experiment.Branch("1", 2)
        )
        val evaluator = ExperimentEvaluator()
        evaluator.selectBranchByWeight(2, branches)
    }

    @Test(expected = AssertionError::class)
    fun `test selectBranchByWeight() asserts lower out-of-bounds`() {
        val branches = listOf(
            Experiment.Branch("1", 2)
        )
        val evaluator = ExperimentEvaluator()
        evaluator.selectBranchByWeight(2, branches)
    }

    @Test
    fun `test pickActiveBranch()`() {
        val e = ExperimentEvaluator()

        var experiment = createDefaultExperiment(
            branches = listOf(
                Experiment.Branch("1", 2)
            )
        )

        assertEquals("1", e.pickActiveBranch(experiment) { _, _ -> 0 }.name)
        assertEquals("1", e.pickActiveBranch(experiment) { _, _ -> 1 }.name)

        experiment = createDefaultExperiment(
            branches = listOf(
                Experiment.Branch("1", 1),
                Experiment.Branch("2", 2)
            )
        )

        assertEquals("1", e.pickActiveBranch(experiment) { _, _ -> 0 }.name)
        assertEquals("2", e.pickActiveBranch(experiment) { _, _ -> 1 }.name)
        assertEquals("2", e.pickActiveBranch(experiment) { _, _ -> 2 }.name)
    }

    private fun evaluate(
        diceValue: Int,
        descriptor: ExperimentDescriptor,
        experiments: List<Experiment>
    ): ActiveExperiment? {
        val e = ExperimentEvaluator(object : ValuesProvider() {
            override fun getRandomBranchValue(lower: Int, upper: Int): Int {
                return diceValue
            }
            override fun getClientId(context: Context): String = "c641eacf-c30c-4171-b403-f077724e848a"
        })
        return e.evaluate(context, descriptor, experiments)
    }

    @Test
    fun `test evaluating branch choices`() {
        testReset()

        var experiments = listOf(
            createDefaultExperiment(
                id = "id",
                branches = listOf(
                    Experiment.Branch("1", 2)
                )
            )
        )
        val descriptor = ExperimentDescriptor("id")
        assertEquals(
            ActiveExperiment(experiments[0], "1"),
            evaluate(0, descriptor, experiments)!!
        )
        assertEquals(
            ActiveExperiment(experiments[0], "1"),
            evaluate(0, descriptor, experiments)!!
        )

        experiments = listOf(
            createDefaultExperiment(
                id = "other"
            ),
            createDefaultExperiment(
                id = "id",
                branches = listOf(
                    Experiment.Branch("1", 1),
                    Experiment.Branch("2", 1)
                )
            )
        )
        assertEquals(
            ActiveExperiment(experiments[1], "1"),
            evaluate(0, descriptor, experiments)!!
        )
        assertEquals(
            ActiveExperiment(experiments[1], "2"),
            evaluate(1, descriptor, experiments)!!
        )
    }

    private fun evaluateWithDebugTag(
        debugTag: String?,
        descriptor: ExperimentDescriptor,
        experiments: List<Experiment>
    ): ActiveExperiment? {
        val e = ExperimentEvaluator(object : ValuesProvider() {
            override fun getDebugTag(): String? {
                return debugTag
            }
            override fun getClientId(context: Context): String = "c641eacf-c30c-4171-b403-f077724e848a"
        })
        return e.evaluate(context, descriptor, experiments)
    }

    @Test
    fun `evaluate debug tags`() {
        testReset()

        val experiments = listOf(
            createDefaultExperiment(
                id = "id-1",
                match = createDefaultMatcher(
                    debugTags = listOf("tag-1")
                )
            ),
            createDefaultExperiment(
                id = "id-2",
                match = createDefaultMatcher(
                    debugTags = listOf("tag-2", "tag-3")
                )
            )
        )
        val descriptor1 = ExperimentDescriptor("id-1")
        val descriptor2 = ExperimentDescriptor("id-2")

        assertNull(evaluateWithDebugTag("other-tag", descriptor1, experiments))
        assertNull(evaluateWithDebugTag("other-tag", descriptor2, experiments))
        assertEquals(
            ActiveExperiment(experiments[0], "only-branch"),
            evaluateWithDebugTag("tag-1", descriptor1, experiments)!!
        )
        assertEquals(
            ActiveExperiment(experiments[1], "only-branch"),
            evaluateWithDebugTag("tag-2", descriptor2, experiments)!!
        )
    }
}
