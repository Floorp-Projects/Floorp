/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.spyk
import io.mockk.verify
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.FenixApplication
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.utils.Settings

internal class FirstSessionPingTest {

    @Test
    fun `checkAndSend() triggers the ping if it wasn't marked as triggered`() {
        val mockedPackageManager: PackageManager = mockk(relaxed = true)
        mockedPackageManager.configureMockInstallSourcePackage()

        val mockedApplication: FenixApplication = mockk(relaxed = true)
        every { mockedApplication.packageManager } returns mockedPackageManager

        val mockedContext: Context = mockk(relaxed = true)
        every { mockedContext.applicationContext } returns mockedApplication

        val mockedSettings: Settings = mockk(relaxed = true)
        mockkStatic("org.mozilla.fenix.ext.ContextKt")
        every { mockedContext.settings() } returns mockedSettings

        val mockAp = spyk(FirstSessionPing(mockedContext), recordPrivateCalls = true)
        every { mockAp.checkMetricsNotEmpty() } returns true
        every { mockAp.wasAlreadyTriggered() } returns false
        every { mockAp.markAsTriggered() } just Runs

        mockAp.checkAndSend()

        verify(exactly = 1) { mockAp.triggerPing() }
        // Marking the ping as triggered happens in a co-routine off the main thread,
        // so wait a bit for it.
        verify(timeout = 5000, exactly = 1) { mockAp.markAsTriggered() }
    }

    @Test
    fun `checkAndSend() doesn't trigger the ping again if it was marked as triggered`() {
        val mockAp = spyk(FirstSessionPing(mockk()), recordPrivateCalls = true)
        every { mockAp.wasAlreadyTriggered() } returns true

        mockAp.checkAndSend()

        verify(exactly = 0) { mockAp.triggerPing() }
    }

    @Test
    fun `WHEN build version is R installSourcePackage RETURNS the set package name`() {
        val mockedPackageManager: PackageManager = mockk(relaxed = true)
        val testPackageName = "test R"
        mockedPackageManager.mockInstallSourcePackageForBuildMinR(testPackageName)

        val mockedApplication: FenixApplication = mockk(relaxed = true)
        every { mockedApplication.packageManager } returns mockedPackageManager

        val mockedContext: Context = mockk(relaxed = true)
        every { mockedContext.applicationContext } returns mockedApplication

        val result = FirstSessionPing(mockedContext).installSourcePackage(Build.VERSION_CODES.R)
        assertEquals(testPackageName, result)
    }

    @Test
    fun `GIVEN packageManager throws an exception WHEN Build version is R installSourcePackage RETURNS an empty string`() {
        val mockedPackageManager: PackageManager = mockk(relaxed = true)
        every { mockedPackageManager.getInstallSourceInfo(any()).installingPackageName } throws PackageManager.NameNotFoundException()

        val mockedApplication: FenixApplication = mockk(relaxed = true)
        every { mockedApplication.packageManager } returns mockedPackageManager

        val mockedContext: Context = mockk(relaxed = true)
        every { mockedContext.applicationContext } returns mockedApplication

        val result = FirstSessionPing(mockedContext).installSourcePackage(Build.VERSION_CODES.R)
        assertEquals("", result)
    }

    @Test
    fun `WHEN build version is more than R installSourcePackage RETURNS the set package name`() {
        val mockedPackageManager: PackageManager = mockk(relaxed = true)
        val testPackageName = "test > R"
        mockedPackageManager.mockInstallSourcePackageForBuildMinR(testPackageName)

        val mockedApplication: FenixApplication = mockk(relaxed = true)
        every { mockedApplication.packageManager } returns mockedPackageManager

        val mockedContext: Context = mockk(relaxed = true)
        every { mockedContext.applicationContext } returns mockedApplication

        val result =
            FirstSessionPing(mockedContext).installSourcePackage(Build.VERSION_CODES.R.plus(1))
        assertEquals(testPackageName, result)
    }

    @Test
    fun `GIVEN packageManager throws an exception WHEN Build version is more than R installSourcePackage RETURNS an empty string`() {
        val mockedPackageManager: PackageManager = mockk(relaxed = true)
        every { mockedPackageManager.getInstallSourceInfo(any()).installingPackageName } throws PackageManager.NameNotFoundException()

        val mockedApplication: FenixApplication = mockk(relaxed = true)
        every { mockedApplication.packageManager } returns mockedPackageManager

        val mockedContext: Context = mockk(relaxed = true)
        every { mockedContext.applicationContext } returns mockedApplication

        val result =
            FirstSessionPing(mockedContext).installSourcePackage(Build.VERSION_CODES.R.plus(1))
        assertEquals("", result)
    }

    @Test
    fun `WHEN build version is less than R installSourcePackage RETURNS the set package name`() {
        val mockedPackageManager: PackageManager = mockk(relaxed = true)
        val testPackageName = "test < R"
        mockedPackageManager.mockInstallSourcePackageForBuildMaxQ(testPackageName)

        val mockedApplication: FenixApplication = mockk(relaxed = true)
        every { mockedApplication.packageManager } returns mockedPackageManager

        val mockedContext: Context = mockk(relaxed = true)
        every { mockedContext.applicationContext } returns mockedApplication

        val result =
            FirstSessionPing(mockedContext).installSourcePackage(Build.VERSION_CODES.R.minus(1))
        assertEquals(testPackageName, result)
    }

    @Test
    fun `GIVEN packageManager throws an exception WHEN Build version is less than R installSourcePackage RETURNS an empty string`() {
        val mockedPackageManager: PackageManager = mockk(relaxed = true)
        @Suppress("DEPRECATION")
        every { mockedPackageManager.getInstallerPackageName(any()) } throws IllegalArgumentException()

        val mockedApplication: FenixApplication = mockk(relaxed = true)
        every { mockedApplication.packageManager } returns mockedPackageManager

        val mockedContext: Context = mockk(relaxed = true)
        every { mockedContext.applicationContext } returns mockedApplication

        val result =
            FirstSessionPing(mockedContext).installSourcePackage(Build.VERSION_CODES.R.minus(1))
        assertEquals("", result)
    }
}

private fun PackageManager.configureMockInstallSourcePackage() =
    if (SDK_INT >= Build.VERSION_CODES.R) {
        mockInstallSourcePackageForBuildMinR()
    } else {
        mockInstallSourcePackageForBuildMaxQ()
    }

private fun PackageManager.mockInstallSourcePackageForBuildMinR(packageName: String = "") =
    every { getInstallSourceInfo(any()).installingPackageName } returns packageName

@Suppress("DEPRECATION")
private fun PackageManager.mockInstallSourcePackageForBuildMaxQ(packageName: String = "") =
    every { getInstallerPackageName(any()) } returns packageName
