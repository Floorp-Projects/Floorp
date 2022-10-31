/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.tooling.detekt

import io.gitlab.arturbosch.detekt.test.lint
import org.junit.Assert.assertEquals
import org.junit.Test

class ProjectLicenseRuleTest {

    @Test
    fun testAbsentLicense() {
        val findings = ProjectLicenseRule().lint(fileContent)

        assertEquals(1, findings.size)
        assertEquals(
            "Expected license not found or incorrect in the file: Test.kt.",
            findings.first().message,
        )
    }

    @Test
    fun testInvalidLicense() {
        val file = """
            |/* This Source Code Form is subject to the terms of the Mozilla Public License.
            | * You can obtain one at http://mozilla.org/MPL/2.0/. */
            |
            $fileContent
        """.trimMargin()
        val findings = ProjectLicenseRule().lint(file)

        assertEquals(1, findings.size)
        assertEquals(
            "Expected license not found or incorrect in the file: Test.kt.",
            findings.first().message,
        )
    }

    @Test
    fun testValidLicense() {
        val file = """
            |/* This Source Code Form is subject to the terms of the Mozilla Public
            | * License, v. 2.0. If a copy of the MPL was not distributed with this
            | * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
            |
            $fileContent
        """.trimMargin()
        val findings = ProjectLicenseRule().lint(file)

        assertEquals(0, findings.size)
    }
}

private val fileContent = """
    |package my.package
    |
    |/** My awesome class */
    |class MyClass () {
    |    fun foo () {}
    |}
""".trimMargin()
