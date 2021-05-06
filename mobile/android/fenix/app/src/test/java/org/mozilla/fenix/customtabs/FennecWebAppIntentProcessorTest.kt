/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customtabs

import io.mockk.mockk
import mozilla.components.feature.pwa.ManifestStorage
import mozilla.components.feature.tabs.CustomTabsUseCases
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import java.io.File

@RunWith(FenixRobolectricTestRunner::class)
class FennecWebAppIntentProcessorTest {
    @Test
    fun `fennec manifest path - tmp`() {
        val processor = createFennecWebAppIntentProcessor()

        val file = File("/data/local/tmp/dummy_manifest.json")
        assertFalse(processor.isUnderFennecManifestDirectory(file))
    }

    @Test
    fun `fennec manifest path - correct path`() {
        val processor = createFennecWebAppIntentProcessor()

        val file = File(testContext.filesDir.absolutePath + "/mozilla/rkgl5eyc.default/manifests/c311ad28-f331-482f-ba8f-a0fbf2c56a0d.json")
        assertTrue(processor.isUnderFennecManifestDirectory(file))
    }

    @Test
    fun `fennec manifest path - correct path, but other app`() {
        val processor = createFennecWebAppIntentProcessor()

        val file = File("/data/data/org.other.app/files/mozilla/rkgl5eyc.default/manifests/c311ad28-f331-482f-ba8f-a0fbf2c56a0d.json")
        assertFalse(processor.isUnderFennecManifestDirectory(file))
    }

    @Test
    fun `fennec manifest path - root file`() {
        val processor = createFennecWebAppIntentProcessor()

        val file = File("/c311ad28-f331-482f-ba8f-a0fbf2c56a0d.json")
        assertFalse(processor.isUnderFennecManifestDirectory(file))
    }

    @Test
    fun `fennec manifest path - tmp path rebuild`() {
        val processor = createFennecWebAppIntentProcessor()

        val file = File("/data/local/tmp/files/mozilla/rkgl5eyc.default/manifests/c311ad28-f331-482f-ba8f-a0fbf2c56a0d.json")
        assertFalse(processor.isUnderFennecManifestDirectory(file))
    }
}

private fun createFennecWebAppIntentProcessor(): FennecWebAppIntentProcessor {
    val useCase: CustomTabsUseCases = mockk(relaxed = true)
    val storage: ManifestStorage = mockk(relaxed = true)

    return FennecWebAppIntentProcessor(
        testContext,
        useCase,
        storage
    )
}
