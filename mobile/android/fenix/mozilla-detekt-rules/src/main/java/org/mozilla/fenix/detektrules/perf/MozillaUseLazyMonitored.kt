/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.detektrules.perf

import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.Companion.PRIVATE
import io.gitlab.arturbosch.detekt.api.CodeSmell
import io.gitlab.arturbosch.detekt.api.Config
import io.gitlab.arturbosch.detekt.api.Debt
import io.gitlab.arturbosch.detekt.api.Entity
import io.gitlab.arturbosch.detekt.api.Issue
import io.gitlab.arturbosch.detekt.api.Rule
import io.gitlab.arturbosch.detekt.api.Severity
import io.gitlab.arturbosch.detekt.api.internal.PathFilters
import org.jetbrains.kotlin.psi.KtProperty
import org.jetbrains.kotlin.psi.KtPropertyDelegate

private const val FUN_LAZY = "lazy"

private const val ERROR_MESSAGE = "Please use `by lazyMonitored` instead of `by lazy` " +
    "to declare application components (e.g. variables in Components.kt): it contains extra code to " +
    "monitor for performance regressions. This change is not necessary for non-components."

/**
 * Prevents use of lazy in component groups files where lazyMonitored should be used instead.
 */
open class MozillaUseLazyMonitored(config: Config) : Rule(config) {
    override val issue = Issue(
        this::class.java.simpleName,
        Severity.Performance,
        "Prevents use of lazy in component groups files where lazyMonitored should be used instead",
        Debt.FIVE_MINS,
    )

    override val filters = PathFilters.of(
        includes = COMPONENT_GROUP_FILES.map { "**$it" },
        excludes = emptyList(),
    )

    override fun visitPropertyDelegate(delegate: KtPropertyDelegate) {
        super.visitPropertyDelegate(delegate)

        val delegateFunction = delegate.expression?.node?.firstChildNode // the "lazy" in "by lazy { ..."
        if (delegateFunction?.chars == FUN_LAZY) {
            report(CodeSmell(issue, Entity.from(delegate), ERROR_MESSAGE))
        }
    }

    override fun visitProperty(property: KtProperty) {
        super.visitProperty(property)

        // I only expect components to be declared as members, not at the top level or inside block scopes.
        if (!property.isMember) return

        val rightHandSideOfAssignment = property.initializer

        // This is intended to catch `val example = lazy {` or `... lazy(`. It may be possible to
        // make lazy not the first node to the right of the equal sign but it's probably uncommon
        // enough that we don't handle that case.
        val assignedFunction = rightHandSideOfAssignment?.firstChild
        if (assignedFunction?.node?.chars == FUN_LAZY) {
            report(CodeSmell(issue, Entity.from(property), ERROR_MESSAGE))
        }
    }

    companion object {
        /**
         * All files that represent a "component group".
         */
        @VisibleForTesting(otherwise = PRIVATE)
        val COMPONENT_GROUP_FILES = listOf(
            "Analytics",
            "BackgroundServices",
            "Components",
            "Core",
            "IntentProcessors",
            "PerformanceComponent",
            "Push",
            "Services",
            "UseCases",
        ).map { "app/src/main/java/org/mozilla/fenix/components/$it.kt" }
    }
}
