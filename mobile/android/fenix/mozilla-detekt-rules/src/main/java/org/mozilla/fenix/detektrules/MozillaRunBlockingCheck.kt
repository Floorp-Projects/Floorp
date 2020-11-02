/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.detektrules

import io.gitlab.arturbosch.detekt.api.*
import org.jetbrains.kotlin.psi.*
import org.jetbrains.kotlin.resolve.BindingContext
import org.jetbrains.kotlin.resolve.calls.callUtil.getCall
import org.jetbrains.kotlin.resolve.calls.callUtil.getCalleeExpressionIfAny
import kotlin.math.exp

private const val VIOLATION_MSG = "Please use `org.mozilla.fenix.perf.runBlockingImplement` instead" +
        "because it allows us to monitor the code for performance regressions."

/**
 * A check to prevent us from working around mechanisms we implemented in
 * @see org.mozilla.fenix.perf.RunBlockingCounter.runBlockingIncrement to count how many runBlocking
 * are used.
 *
 * IF YOU UPDATE THIS FILE NAME, UPDATE CODE OWNERS.
 */
class MozillaRunBlockingCheck(config: Config) : Rule(config) {

    override val issue = Issue(
        "MozillaRunBlockingCheck",
        Severity.Performance,
        "Prevents us from working around mechanisms we implemented to count how many " +
                "runBlocking are used",
        Debt.TWENTY_MINS
    )

    override fun visitImportDirective(importDirective: KtImportDirective) {
        if (importDirective.importPath?.toString() == "kotlinx.coroutines.runBlocking") {
            report(CodeSmell(issue, Entity.from(importDirective), VIOLATION_MSG))
        }
    }
}
