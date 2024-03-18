/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.content.pm

import android.content.Context
import android.content.pm.PackageInfo
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.robolectric.Shadows.shadowOf

@RunWith(AndroidJUnit4::class)
class PackageManagerTest {
    private fun createContext(
        installedApps: List<String> = emptyList(),
    ): Context {
        val pm = testContext.packageManager
        val packageManager = shadowOf(pm)
        val context = mock<Context>()
        `when`(context.packageManager).thenReturn(pm)
        installedApps.forEach { name ->
            val packageInfo = PackageInfo().apply {
                packageName = name
            }
            packageManager.addPackageNoDefaults(packageInfo)
        }

        return context
    }

    /**
     * Verify that PackageManager.isPackageInstalled works correctly.
     */
    @Test
    fun `isPackageInstalled() returns true when package is installed, false otherwise`() {
        val context = createContext(listOf("com.example", "com.test"))

        assert(context.packageManager.isPackageInstalled("com.example"))
        assert(context.packageManager.isPackageInstalled("com.test"))
        assertFalse(context.packageManager.isPackageInstalled("com.mozilla"))
        assertFalse(context.packageManager.isPackageInstalled("com.example.com"))
    }
}
