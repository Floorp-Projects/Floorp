/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.tooling.detekt

import io.gitlab.arturbosch.detekt.api.CodeSmell
import io.gitlab.arturbosch.detekt.api.Config
import io.gitlab.arturbosch.detekt.api.Debt
import io.gitlab.arturbosch.detekt.api.Entity
import io.gitlab.arturbosch.detekt.api.Issue
import io.gitlab.arturbosch.detekt.api.Rule
import io.gitlab.arturbosch.detekt.api.Severity
import org.jetbrains.kotlin.psi.KtFile

/**
 * Check header license in Kotlin files.
 */
class ProjectLicenseRule(config: Config = Config.empty) : Rule(config) {

    private val expectedLicense = """
        |/* This Source Code Form is subject to the terms of the Mozilla Public
        | * License, v. 2.0. If a copy of the MPL was not distributed with this
        | * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
    """.trimMargin()

    override val issue = Issue(
        id = "AbsentOrWrongFileLicense",
        severity = Severity.Style,
        description = "License text is absent or incorrect in the file.",
        debt = Debt.FIVE_MINS,
    )

    override fun visitKtFile(file: KtFile) {
        if (!file.hasValidLicense) {
            reportCodeSmell(file)
        }
    }

    private val KtFile.hasValidLicense: Boolean
        get() = text.startsWith(expectedLicense)

    private fun reportCodeSmell(file: KtFile) {
        report(
            CodeSmell(
                issue,
                Entity.from(file),
                "Expected license not found or incorrect in the file: ${file.name}.",
            ),
        )
    }
}
