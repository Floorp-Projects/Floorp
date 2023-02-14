/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.detektrules.perf

import io.gitlab.arturbosch.detekt.api.Config
import io.gitlab.arturbosch.detekt.test.lint
import org.junit.jupiter.api.Assertions.*
import org.junit.jupiter.api.BeforeEach
import org.junit.jupiter.api.Test

internal class MozillaStrictModeSuppressionTest {

    private lateinit var check: MozillaStrictModeSuppression

    @BeforeEach
    fun setUp() {
        check = MozillaStrictModeSuppression(Config.empty)
    }

    @Test
    fun `GIVEN a file with import not resetAfter WHEN linting THEN pass`() {
        val file = """import android.os.StrictMode"""
        assertNoFinding(file)
    }

    @Test
    fun `GIVEN a file with import ac resetAfter WHEN linting THEN fail`() {
        val file = """import mozilla.components.support.ktx.android.os.resetAfter"""
        assertOneFinding(file)
    }

    @Test
    fun `GIVEN a file with StrictMode getVmPolicy WHEN linting THEN pass`() {
        val file = """fun main() {
            |     val policy = StrictMode.getVmPolicy()
            | }""".trimMargin()
        assertNoFinding(file)
    }

    @Test
    fun `GIVEN a file with StrictMode setVmPolicy WHEN linting THEN fail`() {
        val file = """fun main() {
            |     val policy = StrictMode.VmPolicy.Builder.build()
            |     StrictMode.setVmPolicy(policy)
            | }""".trimMargin()
        assertOneFinding(file)
    }

    @Test
    fun `GIVEN a file with StrictMode setThreadPolicy WHEN linting THEN fail`() {
        val file = """fun main() {
            |     val policy = StrictMode.ThreadPolicy.Builder.build()
            |     StrictMode.setThreadPolicy(policy)
            | }""".trimMargin()
        assertOneFinding(file)
    }

    private fun assertNoFinding(fileStr: String) {
        val findings = check.lint(fileStr)
        assertEquals(0, findings.size)
    }

    private fun assertOneFinding(fileStr: String) {
        val findings = check.lint(fileStr)
        assertEquals(1, findings.size)
        assertEquals(check.issue, findings.first().issue)
    }
}
