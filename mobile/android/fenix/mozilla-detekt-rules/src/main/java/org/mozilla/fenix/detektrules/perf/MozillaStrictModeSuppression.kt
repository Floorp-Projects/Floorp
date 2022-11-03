/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.detektrules.perf

import io.gitlab.arturbosch.detekt.api.CodeSmell
import io.gitlab.arturbosch.detekt.api.Config
import io.gitlab.arturbosch.detekt.api.Debt
import io.gitlab.arturbosch.detekt.api.Entity
import io.gitlab.arturbosch.detekt.api.Issue
import io.gitlab.arturbosch.detekt.api.Rule
import io.gitlab.arturbosch.detekt.api.Severity
import org.jetbrains.kotlin.psi.KtCallExpression
import org.jetbrains.kotlin.psi.KtImportDirective

private const val VIOLATION_MSG = "Please use `components.strictMode.resetAfter` instead because it has " +
    "performance improvements and additional code to monitor for performance regressions."

/**
 * A check to prevent us from working around mechanisms we implemented to prevent suppressing StrictMode.
 */
class MozillaStrictModeSuppression(config: Config) : Rule(config) {
    override val issue = Issue(
        "MozillaStrictModeSuppression",
        Severity.Performance,
        "Prevents us from working around mechanisms we implemented to prevent suppressing StrictMode",
        Debt.TEN_MINS,
    )

    override fun visitImportDirective(importDirective: KtImportDirective) {
        super.visitImportDirective(importDirective)
        reportIfImportAcResetAfter(importDirective)
    }

    override fun visitCallExpression(expression: KtCallExpression) {
        super.visitCallExpression(expression)
        reportIfCallStrictModeSetPolicy(expression)
    }

    private fun reportIfImportAcResetAfter(importDirective: KtImportDirective) {
        if (importDirective.importPath?.toString() == "mozilla.components.support.ktx.android.os.resetAfter") {
            report(CodeSmell(issue, Entity.from(importDirective), VIOLATION_MSG))
        }
    }

    private fun reportIfCallStrictModeSetPolicy(expression: KtCallExpression) {
        // There is probably a more correct way of doing this but this API is not well documented
        // so it's not worth our time, I think.
        val receiver = expression.parent?.firstChild?.node?.chars
        val calledMethod = expression.calleeExpression?.firstChild?.node?.chars

        // This won't catch if setVmPolicy is imported directly. However, this is unlikely so let's
        // not handle it now. Maybe we can add it when we add tests for this file.
        if (receiver == "StrictMode") {
            val violationMsg = when (calledMethod) {
                "setThreadPolicy" -> VIOLATION_MSG
                "setVmPolicy" ->
                    "NOT YET IMPLEMENTED: please consult the perf team about implementing" +
                        "`StrictModeManager.resetAfter`: we want to understand the performance implications " +
                        "of suppressing setVmPolicy before allowing it."
                else -> null
            }

            violationMsg?.let { msg ->
                report(CodeSmell(issue, Entity.from(expression), msg))
            }
        }
    }
}
