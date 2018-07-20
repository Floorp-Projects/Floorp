/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard

import android.content.Context
import android.content.SharedPreferences
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.ArgumentMatchers.anyString
import org.mockito.ArgumentMatchers.eq
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class ExperimentEvaluatorTest {
    @Test
    fun testEvaluateBuckets() {
        val experiment = Experiment(
            "testid",
            "testexperiment",
            "testdesc",
            Experiment.Matcher(
                "eng",
                "test.appId",
                listOf("USA", "GBR"),
                "test.version",
                "unknown",
                "robolectric",
                "USA"
            ),
            Experiment.Bucket(
                70,
                20
            ),
            1528916183)

        val context = mock(Context::class.java)
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPreferences = mock(SharedPreferences::class.java)
        `when`(sharedPreferences.getBoolean(eq("testid"), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val packageManager = mock(PackageManager::class.java)
        val packageInfo = PackageInfo()
        packageInfo.versionName = "test.version"
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val evaluator = ExperimentEvaluator()
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 21))
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 69))
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 19))
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 70))
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 71))
    }

    @Test
    fun testEvaluateAppId() {
        val experiment = Experiment(
            "testid",
            "testexperiment",
            "testdesc",
            Experiment.Matcher(
                "eng",
                "^test$",
                listOf("USA", "GBR"),
                "test.version",
                "unknown",
                "robolectric",
                "USA"
            ),
            Experiment.Bucket(
                70,
                20
            ),
            1528916183)

        val context = mock(Context::class.java)
        `when`(context.packageName).thenReturn("other.appId")
        val sharedPreferences = mock(SharedPreferences::class.java)
        `when`(sharedPreferences.getBoolean(eq("testid"), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val packageManager = mock(PackageManager::class.java)
        val packageInfo = PackageInfo()
        packageInfo.versionName = "test.version"
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val evaluator = ExperimentEvaluator()
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))
        `when`(context.packageName).thenReturn("test")
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))
    }

    @Test
    fun testEvaluateLanguage() {
        var experiment = Experiment(
            "testid",
            "testexperiment",
            "testdesc",
            Experiment.Matcher(
                "eng",
                "test.appId",
                listOf("USA", "GBR"),
                "test.version",
                "unknown",
                "robolectric",
                "USA"
            ),
            Experiment.Bucket(
                70,
                20
            ),
            1528916183)

        val context = mock(Context::class.java)
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPreferences = mock(SharedPreferences::class.java)
        `when`(sharedPreferences.getBoolean(eq("testid"), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val packageManager = mock(PackageManager::class.java)
        val packageInfo = PackageInfo()
        packageInfo.versionName = "test.version"
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val evaluator = ExperimentEvaluator()
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))
        experiment = Experiment(
            "testid",
            "testexperiment",
            "testdesc",
            Experiment.Matcher(
                "esp",
                "test.appId",
                listOf("USA", "GBR"),
                "test.version",
                "unknown",
                "robolectric",
                "USA"
            ),
            Experiment.Bucket(
                70,
                20
            ),
            1528916183)
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))
    }

    @Test
    fun testEvaluateCountry() {
        var experiment = Experiment(
            "testid",
            "testexperiment",
            "testdesc",
            Experiment.Matcher(
                "eng",
                "test.appId",
                listOf("USA", "GBR"),
                "test.version",
                "unknown",
                "robolectric",
                "USA"
            ),
            Experiment.Bucket(
                70,
                20
            ),
            1528916183)

        val context = mock(Context::class.java)
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPreferences = mock(SharedPreferences::class.java)
        `when`(sharedPreferences.getBoolean(eq("testid"), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val packageManager = mock(PackageManager::class.java)
        val packageInfo = PackageInfo()
        packageInfo.versionName = "test.version"
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val evaluator = ExperimentEvaluator()
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))

        experiment = Experiment(
            "testid",
            "testexperiment",
            "testdesc",
            Experiment.Matcher(
                "eng",
                "test.appId",
                listOf("USA", "GBR"),
                "test.version",
                "unknown",
                "robolectric",
                "ESP"
            ),
            Experiment.Bucket(
                70,
                20
            ),
            1528916183)

        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))
    }

    @Test
    fun testEvaluateVersion() {
        val experiment = Experiment(
            "testid",
            "testexperiment",
            "testdesc",
            Experiment.Matcher(
                "eng",
                "test.appId",
                listOf("USA", "GBR"),
                "test.version",
                "unknown",
                "robolectric",
                "USA"
            ),
            Experiment.Bucket(
                70,
                20
            ),
            1528916183)

        val context = mock(Context::class.java)
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPreferences = mock(SharedPreferences::class.java)
        `when`(sharedPreferences.getBoolean(eq("testid"), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val packageManager = mock(PackageManager::class.java)
        val packageInfo = PackageInfo()
        packageInfo.versionName = "test.version"
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val evaluator = ExperimentEvaluator()
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))

        packageInfo.versionName = "other.version"
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))
    }

    @Test
    fun testEvaluateDevice() {
        var experiment = Experiment(
            "testid",
            "testexperiment",
            "testdesc",
            Experiment.Matcher(
                "eng",
                "test.appId",
                listOf("USA", "GBR"),
                "test.version",
                "unknown",
                "robolectric",
                "USA"
            ),
            Experiment.Bucket(
                70,
                20
            ),
            1528916183)

        val context = mock(Context::class.java)
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPreferences = mock(SharedPreferences::class.java)
        `when`(sharedPreferences.getBoolean(eq("testid"), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val packageManager = mock(PackageManager::class.java)
        val packageInfo = PackageInfo()
        packageInfo.versionName = "test.version"
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val evaluator = ExperimentEvaluator()
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))

        experiment = Experiment(
            "testid",
            "testexperiment",
            "testdesc",
            Experiment.Matcher(
                "eng",
                "test.appId",
                listOf("USA", "GBR"),
                "test.version",
                "unknown",
                "otherdevice",
                "USA"
            ),
            Experiment.Bucket(
                70,
                20
            ),
            1528916183)

        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))
    }

    @Test
    fun testEvaluateManufacturer() {
        var experiment = Experiment(
            "testid",
            "testexperiment",
            "testdesc",
            Experiment.Matcher(
                "eng",
                "test.appId",
                listOf("USA", "GBR"),
                "test.version",
                "unknown",
                "robolectric",
                "USA"
            ),
            Experiment.Bucket(
                70,
                20
            ),
            1528916183)

        val context = mock(Context::class.java)
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPreferences = mock(SharedPreferences::class.java)
        `when`(sharedPreferences.getBoolean(eq("testid"), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val packageManager = mock(PackageManager::class.java)
        val packageInfo = PackageInfo()
        packageInfo.versionName = "test.version"
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val evaluator = ExperimentEvaluator()
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))

        experiment = Experiment(
            "testid",
            "testexperiment",
            "testdesc",
            Experiment.Matcher(
                "eng",
                "test.appId",
                listOf("USA", "GBR"),
                "test.version",
                "other",
                "robolectric",
                "USA"
            ),
            Experiment.Bucket(
                70,
                20
            ),
            1528916183)

        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))
    }

    @Test
    fun testEvaluateRegion() {
        val experiment = Experiment(
            "testid",
            "testexperiment",
            "testdesc",
            Experiment.Matcher(
                "eng",
                "test.appId",
                listOf("USA", "GBR"),
                "test.version",
                "unknown",
                "robolectric",
                "USA"
            ),
            Experiment.Bucket(
                70,
                20
            ),
            1528916183)

        val context = mock(Context::class.java)
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPreferences = mock(SharedPreferences::class.java)
        `when`(sharedPreferences.getBoolean(eq("testid"), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val packageManager = mock(PackageManager::class.java)
        val packageInfo = PackageInfo()
        packageInfo.versionName = "test.version"
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        var evaluator = ExperimentEvaluator(object : ValuesProvider() {
            override fun getRegion(context: Context): String? {
                return "USA"
            }
        })

        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))

        evaluator = ExperimentEvaluator(object : ValuesProvider() {
            override fun getRegion(context: Context): String? {
                return "ESP"
            }
        })

        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))
    }

    @Test
    fun testEvaluateActivateOverride() {
        val context = mock(Context::class.java)
        val sharedPreferences = mock(SharedPreferences::class.java)
        `when`(sharedPreferences.getBoolean(eq("id"), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val evaluator = ExperimentEvaluator()
        val experiment = Experiment("id", bucket = Experiment.Bucket(100, 0))
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("id"), listOf(experiment), -1))
        `when`(sharedPreferences.getBoolean(eq("id"), anyBoolean())).thenReturn(true)
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("id"), listOf(experiment), -1))
    }

    @Test
    fun testEvaluateDeactivateOverride() {
        val context = mock(Context::class.java)
        val sharedPreferences = mock(SharedPreferences::class.java)
        `when`(sharedPreferences.getBoolean(eq("id"), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val evaluator = ExperimentEvaluator()
        val experiment = Experiment("id", bucket = Experiment.Bucket(100, 0))
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("id"), listOf(experiment), 50))
        `when`(sharedPreferences.getBoolean(eq("id"), anyBoolean())).thenReturn(false)
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("id"), listOf(experiment), 50))
    }

    @Test
    fun testEvaluateNoExperimentSameAsDescriptor() {
        val savedExperiment = Experiment("wrongid")
        val descriptor = ExperimentDescriptor("testid")
        val context = mock(Context::class.java)
        assertNull(ExperimentEvaluator().evaluate(context, descriptor, listOf(savedExperiment), 20))
    }

    @Test
    fun testSetOverrideActivate() {
        val context = mock(Context::class.java)
        val sharedPreferences = mock(SharedPreferences::class.java)
        val sharedPreferencesEditor = mock(SharedPreferences.Editor::class.java)
        `when`(sharedPreferencesEditor.putBoolean(anyString(), anyBoolean())).thenReturn(sharedPreferencesEditor)
        `when`(sharedPreferences.edit()).thenReturn(sharedPreferencesEditor)
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val evaluator = ExperimentEvaluator()
        evaluator.setOverride(context, ExperimentDescriptor("exp-id"), true)
        verify(sharedPreferencesEditor).putBoolean("exp-id", true)
    }

    @Test
    fun testSetOverrideDeactivate() {
        val context = mock(Context::class.java)
        val sharedPreferences = mock(SharedPreferences::class.java)
        val sharedPreferencesEditor = mock(SharedPreferences.Editor::class.java)
        `when`(sharedPreferencesEditor.putBoolean(eq("exp-2-id"), anyBoolean())).thenReturn(sharedPreferencesEditor)
        `when`(sharedPreferences.edit()).thenReturn(sharedPreferencesEditor)
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val evaluator = ExperimentEvaluator()
        evaluator.setOverride(context, ExperimentDescriptor("exp-2-id"), false)
        verify(sharedPreferencesEditor).putBoolean("exp-2-id", false)
    }

    @Test
    fun testClearOverride() {
        val context = mock(Context::class.java)
        val sharedPreferences = mock(SharedPreferences::class.java)
        val sharedPreferencesEditor = mock(SharedPreferences.Editor::class.java)
        `when`(sharedPreferencesEditor.remove(anyString())).thenReturn(sharedPreferencesEditor)
        `when`(sharedPreferences.edit()).thenReturn(sharedPreferencesEditor)
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val evaluator = ExperimentEvaluator()
        evaluator.clearOverride(context, ExperimentDescriptor("exp-id"))
        verify(sharedPreferencesEditor).remove("exp-id")
    }

    @Test
    fun testClearAllOverrides() {
        val context = mock(Context::class.java)
        val sharedPreferences = mock(SharedPreferences::class.java)
        val sharedPreferencesEditor = mock(SharedPreferences.Editor::class.java)
        `when`(sharedPreferencesEditor.clear()).thenReturn(sharedPreferencesEditor)
        `when`(sharedPreferences.edit()).thenReturn(sharedPreferencesEditor)
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val evaluator = ExperimentEvaluator()
        evaluator.clearAllOverrides(context)
        verify(sharedPreferencesEditor).clear()
    }
}