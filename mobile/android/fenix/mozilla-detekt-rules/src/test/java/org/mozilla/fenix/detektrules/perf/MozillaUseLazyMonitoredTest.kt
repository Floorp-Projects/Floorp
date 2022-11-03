/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.detektrules.perf

import io.gitlab.arturbosch.detekt.api.Config
import io.gitlab.arturbosch.detekt.api.internal.PathFilters
import io.gitlab.arturbosch.detekt.test.lint
import org.junit.jupiter.api.Assertions.*
import org.junit.jupiter.api.BeforeEach
import org.junit.jupiter.api.Test
import org.junit.jupiter.params.ParameterizedTest
import org.junit.jupiter.params.provider.ValueSource
import java.io.File
import java.nio.file.Paths

internal class MozillaUseLazyMonitoredTest {

    private lateinit var actualCheck: MozillaUseLazyMonitored
    private lateinit var check: TestMozillaUseLazyMonitored

    @BeforeEach
    fun setUp() {
        actualCheck = MozillaUseLazyMonitored(Config.empty)
        check = TestMozillaUseLazyMonitored()
    }

    @Test
    fun `GIVEN the hard-coded component group file paths THEN there are some to run the lint on`() {
        assertFalse(MozillaUseLazyMonitored.COMPONENT_GROUP_FILES.isEmpty())
    }

    @Test
    fun `GIVEN the hard-coded component group file paths THEN they all exist`() {
        MozillaUseLazyMonitored.COMPONENT_GROUP_FILES.forEach { path ->
            // The tests working directory is `<repo>/mozilla-detekt-rules` so we need to back out.
            val modifiedPath = "../$path"

            val errorMessage = "This lint rule hard-codes paths to files containing \"component groups\". " +
                "One of these files - $path - no longer exists. Please update the check " +
                "${MozillaUseLazyMonitored::class.java.simpleName} with the new path."
            assertTrue(File(modifiedPath).exists(), errorMessage)
        }
    }

    @Test
    fun `GIVEN the hard-coded component group file paths THEN none of the files are ignored`() {
        MozillaUseLazyMonitored.COMPONENT_GROUP_FILES.forEach {
            assertTrue(actualCheck.filters?.isIgnored(Paths.get(it)) == false)
        }
    }

    @Test
    fun `GIVEN a snippet with lazyMonitored THEN there are no findings`() {
        val text = getCodeInClass("val example by lazyMonitored { 4 }")
        assertNoFindings(text)
    }

    @ParameterizedTest
    @ValueSource(
        strings = [
            "val example by lazy { 4 }",
            "val example    by    lazy    {    4    }",
            "val example\nby\nlazy\n{\n4\n}",
        ],
    )
    fun `GIVEN a snippet with by lazy THEN there is a finding`(innerText: String) {
        val text = getCodeInClass(innerText)
        assertOneFinding(text)
    }

    @ParameterizedTest
    @ValueSource(
        strings = [
            "val example = lazy(::exampleFun)",
            "val example   =   lazy   (  ::exampleFun)",
            "val example\n=\nlazy\n(\n::exampleFun\n)",
        ],
    )
    fun `GIVEN a snippet with = lazy THEN there is a finding`(innerText: String) {
        val text = getCodeInClass(innerText)
        assertOneFinding(text)
    }

    private fun assertNoFindings(text: String) {
        val findings = check.lint(text)
        assertEquals(0, findings.size)
    }

    private fun assertOneFinding(text: String) {
        val findings = check.lint(text)
        assertEquals(1, findings.size)
        assertEquals(check.issue, findings[0].issue)
    }
}

private fun getCodeInClass(text: String): String = """class Example() {
    $text
}"""

class TestMozillaUseLazyMonitored : MozillaUseLazyMonitored(Config.empty) {
    // If left on their own, the path filters will prevent us from running checks on raw strings.
    // I'm not sure the best way to work around that so we just override the value.
    override val filters: PathFilters? = null
}
