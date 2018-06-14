/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard

import android.content.Context
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
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
        val packageManager = mock(PackageManager::class.java)
        val packageInfo = PackageInfo()
        packageInfo.versionName = "test.version"
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val evaluator = ExperimentEvaluator()
        assertTrue(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))
        assertTrue(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 21))
        assertTrue(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 69))
        assertFalse(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 19))
        assertFalse(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 70))
        assertFalse(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 71))
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
        val packageManager = mock(PackageManager::class.java)
        val packageInfo = PackageInfo()
        packageInfo.versionName = "test.version"
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val evaluator = ExperimentEvaluator()
        assertFalse(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))
        `when`(context.packageName).thenReturn("test")
        assertTrue(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))
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
        val packageManager = mock(PackageManager::class.java)
        val packageInfo = PackageInfo()
        packageInfo.versionName = "test.version"
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val evaluator = ExperimentEvaluator()
        assertTrue(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))
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
        assertFalse(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))
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
        val packageManager = mock(PackageManager::class.java)
        val packageInfo = PackageInfo()
        packageInfo.versionName = "test.version"
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val evaluator = ExperimentEvaluator()
        assertTrue(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))

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

        assertFalse(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))
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
        val packageManager = mock(PackageManager::class.java)
        val packageInfo = PackageInfo()
        packageInfo.versionName = "test.version"
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val evaluator = ExperimentEvaluator()
        assertTrue(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))

        packageInfo.versionName = "other.version"
        assertFalse(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))
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
        val packageManager = mock(PackageManager::class.java)
        val packageInfo = PackageInfo()
        packageInfo.versionName = "test.version"
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val evaluator = ExperimentEvaluator()
        assertTrue(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))

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

        assertFalse(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))
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
        val packageManager = mock(PackageManager::class.java)
        val packageInfo = PackageInfo()
        packageInfo.versionName = "test.version"
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val evaluator = ExperimentEvaluator()
        assertTrue(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))

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

        assertFalse(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))
    }

    @Test
    fun testEvaluateRegion() {
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
        val packageManager = mock(PackageManager::class.java)
        val packageInfo = PackageInfo()
        packageInfo.versionName = "test.version"
        `when`(packageManager.getPackageInfo(anyString(), anyInt())).thenReturn(packageInfo)
        `when`(context.packageManager).thenReturn(packageManager)

        val evaluator = ExperimentEvaluator()
        assertTrue(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))

        experiment = Experiment(
            "testid",
            "testexperiment",
            "testdesc",
            Experiment.Matcher(
                "eng",
                "test.appId",
                listOf("GBR"),
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

        assertFalse(evaluator.evaluate(context, ExperimentDescriptor("testid"), listOf(experiment), 20))
    }

    @Test
    fun testEvaluateNoExperimentSameAsDescriptor() {
        val savedExperiment = Experiment("wrongid")
        val descriptor = ExperimentDescriptor("testid")
        val context = mock(Context::class.java)
        assertFalse(ExperimentEvaluator().evaluate(context, descriptor, listOf(savedExperiment), 20))
    }
}