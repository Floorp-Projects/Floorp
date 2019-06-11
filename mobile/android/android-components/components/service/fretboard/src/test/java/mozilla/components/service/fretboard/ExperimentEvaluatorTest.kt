/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard

import android.content.Context
import android.content.SharedPreferences
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
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
import org.mockito.Mockito.mock
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class ExperimentEvaluatorTest {

    @Test
    fun evaluateEmtpyMatchers() {
        val experiment = Experiment(
            "testid",
            "testexperiment",
            "testdesc",
            Experiment.Matcher(
                appId = "",
                regions = listOf()
            ),
            Experiment.Bucket(
                70,
                20
            ),
            1528916183)

        val context = mock<Context>()
        `when`(context.packageName).thenReturn("other.appId")
        val sharedPreferences = mock<SharedPreferences>()
        `when`(sharedPreferences.getBoolean(eq("testexperiment"), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val packageManager = mock<PackageManager>()
        val packageInfo = PackageInfo()
        packageInfo.versionName = "test.version"
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val evaluator = ExperimentEvaluator()
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))
    }

    @Test
    fun evaluateBuckets() {
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

        val context = mock<Context>()
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPreferences = mock<SharedPreferences>()
        `when`(sharedPreferences.getBoolean(eq("testexperiment"), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val packageManager = mock<PackageManager>()
        val packageInfo = PackageInfo()
        packageInfo.versionName = "test.version"
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val evaluator = ExperimentEvaluator()
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 21))
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 69))
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 19))
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 70))
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 71))
    }

    @Test
    fun evaluateAppId() {
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

        val context = mock<Context>()
        `when`(context.packageName).thenReturn("other.appId")
        val sharedPreferences = mock<SharedPreferences>()
        `when`(sharedPreferences.getBoolean(eq("testexperiment"), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val packageManager = mock<PackageManager>()
        val packageInfo = PackageInfo()
        packageInfo.versionName = "test.version"
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val evaluator = ExperimentEvaluator()
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))
        `when`(context.packageName).thenReturn("test")
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))
    }

    @Test
    fun evaluateLanguage() {
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

        val context = mock<Context>()
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPreferences = mock<SharedPreferences>()
        `when`(sharedPreferences.getBoolean(eq("testexperiment"), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val packageManager = mock<PackageManager>()
        val packageInfo = PackageInfo()
        packageInfo.versionName = "test.version"
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val evaluator = ExperimentEvaluator()
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))
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
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))
    }

    @Test
    fun evaluateCountry() {
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

        val context = mock<Context>()
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPreferences = mock<SharedPreferences>()
        `when`(sharedPreferences.getBoolean(eq("testexperiment"), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val packageManager = mock<PackageManager>()
        val packageInfo = PackageInfo()
        packageInfo.versionName = "test.version"
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val evaluator = ExperimentEvaluator()
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

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

        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))
    }

    @Test
    fun evaluateVersion() {
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

        val context = mock<Context>()
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPreferences = mock<SharedPreferences>()
        `when`(sharedPreferences.getBoolean(eq("testexperiment"), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val packageManager = mock<PackageManager>()
        val packageInfo = PackageInfo()
        packageInfo.versionName = "test.version"
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val evaluator = ExperimentEvaluator()
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        packageInfo.versionName = "other.version"
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))
    }

    @Test
    fun evaluateDevice() {
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

        val context = mock<Context>()
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPreferences = mock<SharedPreferences>()
        `when`(sharedPreferences.getBoolean(eq("testexperiment"), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val packageManager = mock<PackageManager>()
        val packageInfo = PackageInfo()
        packageInfo.versionName = "test.version"
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val evaluator = ExperimentEvaluator()
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

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

        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))
    }

    @Test
    fun evaluateReleaseChannel() {
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
                "USA",
                "alpha"
            ),
            Experiment.Bucket(
                70,
                20
            ),
            1528916183)

        val context = mock<Context>()
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPreferences = mock<SharedPreferences>()
        `when`(sharedPreferences.getBoolean(eq("testexperiment"), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val packageManager = mock<PackageManager>()
        val packageInfo = PackageInfo()
        packageInfo.versionName = "test.version"
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        var evaluator = ExperimentEvaluator(object : ValuesProvider() {
            override fun getReleaseChannel(context: Context): String? {
                return "alpha"
            }
        })

        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

        evaluator = ExperimentEvaluator(object : ValuesProvider() {
            override fun getRegion(context: Context): String? {
                return "production"
            }
        })

        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))
    }

    @Test
    fun evaluateManufacturer() {
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

        val context = mock<Context>()
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPreferences = mock<SharedPreferences>()
        `when`(sharedPreferences.getBoolean(eq("testexperiment"), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val packageManager = mock<PackageManager>()
        val packageInfo = PackageInfo()
        packageInfo.versionName = "test.version"
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val evaluator = ExperimentEvaluator()
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))

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

        assertNull(evaluator.evaluate(context, ExperimentDescriptor("testexperiment"), listOf(experiment), 20))
    }

    @Test
    fun evaluateRegion() {
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

        val context = mock<Context>()
        `when`(context.packageName).thenReturn("test.appId")
        val sharedPreferences = mock<SharedPreferences>()
        `when`(sharedPreferences.getBoolean(eq("testexperiment"), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val packageManager = mock<PackageManager>()
        val packageInfo = PackageInfo()
        packageInfo.versionName = "test.version"
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

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
    fun evaluateActivateOverride() {
        val context = mock<Context>()
        val sharedPreferences = mock<SharedPreferences>()
        `when`(sharedPreferences.getBoolean(eq("id"), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val evaluator = ExperimentEvaluator()
        val experiment = Experiment("id", name = "name", bucket = Experiment.Bucket(100, 0))
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("name"), listOf(experiment), -1))
        `when`(sharedPreferences.getBoolean(eq("name"), anyBoolean())).thenReturn(true)
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("name"), listOf(experiment), -1))
    }

    @Test
    fun evaluateDeactivateOverride() {
        val context = mock<Context>()
        val sharedPreferences = mock<SharedPreferences>()
        `when`(sharedPreferences.getBoolean(eq("name"), anyBoolean())).thenAnswer { invocation -> invocation.arguments[1] as Boolean }
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val evaluator = ExperimentEvaluator()
        val experiment = Experiment("id", name = "name", bucket = Experiment.Bucket(100, 0))
        assertNotNull(evaluator.evaluate(context, ExperimentDescriptor("name"), listOf(experiment), 50))
        `when`(sharedPreferences.getBoolean(eq("name"), anyBoolean())).thenReturn(false)
        assertNull(evaluator.evaluate(context, ExperimentDescriptor("name"), listOf(experiment), 50))
    }

    @Test
    fun evaluateNoExperimentSameAsDescriptor() {
        val savedExperiment = Experiment("wrongid", name = "wrongname")
        val descriptor = ExperimentDescriptor("testname")
        val context = mock<Context>()
        assertNull(ExperimentEvaluator().evaluate(context, descriptor, listOf(savedExperiment), 20))
    }

    @Test
    fun setOverrideActivate() {
        val context = mock<Context>()
        val sharedPreferences = mock<SharedPreferences>()
        val sharedPreferencesEditor = mock(SharedPreferences.Editor::class.java)
        `when`(sharedPreferencesEditor.putBoolean(anyString(), anyBoolean())).thenReturn(sharedPreferencesEditor)
        `when`(sharedPreferences.edit()).thenReturn(sharedPreferencesEditor)
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val evaluator = ExperimentEvaluator()
        evaluator.setOverride(context, ExperimentDescriptor("exp-name"), true)
        verify(sharedPreferencesEditor).putBoolean("exp-name", true)
    }

    @Test
    fun setOverrideDeactivate() {
        val context = mock<Context>()
        val sharedPreferences = mock<SharedPreferences>()
        val sharedPreferencesEditor = mock(SharedPreferences.Editor::class.java)
        `when`(sharedPreferencesEditor.putBoolean(eq("exp-2-name"), anyBoolean())).thenReturn(sharedPreferencesEditor)
        `when`(sharedPreferences.edit()).thenReturn(sharedPreferencesEditor)
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val evaluator = ExperimentEvaluator()
        evaluator.setOverride(context, ExperimentDescriptor("exp-2-name"), false)
        verify(sharedPreferencesEditor).putBoolean("exp-2-name", false)
    }

    @Test
    fun clearOverride() {
        val context = mock<Context>()
        val sharedPreferences = mock<SharedPreferences>()
        val sharedPreferencesEditor = mock(SharedPreferences.Editor::class.java)
        `when`(sharedPreferencesEditor.remove(anyString())).thenReturn(sharedPreferencesEditor)
        `when`(sharedPreferences.edit()).thenReturn(sharedPreferencesEditor)
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val evaluator = ExperimentEvaluator()
        evaluator.clearOverride(context, ExperimentDescriptor("exp-name"))
        verify(sharedPreferencesEditor).remove("exp-name")
    }

    @Test
    fun clearAllOverrides() {
        val context = mock<Context>()
        val sharedPreferences = mock<SharedPreferences>()
        val sharedPreferencesEditor = mock(SharedPreferences.Editor::class.java)
        `when`(sharedPreferencesEditor.clear()).thenReturn(sharedPreferencesEditor)
        `when`(sharedPreferences.edit()).thenReturn(sharedPreferencesEditor)
        `when`(context.getSharedPreferences(anyString(), eq(Context.MODE_PRIVATE))).thenReturn(sharedPreferences)
        val evaluator = ExperimentEvaluator()
        evaluator.clearAllOverrides(context)
        verify(sharedPreferencesEditor).clear()
    }

    @Test
    fun overridingClientId() {
        val evaluator1 = ExperimentEvaluator(object : ValuesProvider() {
            override fun getClientId(context: Context): String = "c641eacf-c30c-4171-b403-f077724e848a"
        })

        assertEquals(79, evaluator1.getUserBucket(testContext))

        val evaluator2 = ExperimentEvaluator(object : ValuesProvider() {
            override fun getClientId(context: Context): String = "01a15650-9a5d-4383-a7ba-2f047b25c620"
        })

        assertEquals(55, evaluator2.getUserBucket(testContext))
    }
}