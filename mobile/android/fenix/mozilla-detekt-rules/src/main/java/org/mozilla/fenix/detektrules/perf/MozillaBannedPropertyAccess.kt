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
import org.jetbrains.kotlin.psi.*

class MozillaBannedPropertyAccess(config: Config = Config.empty) : Rule(config) {
    override val issue = Issue(
        "MozillaBannedPropertyAccess",
        Severity.Defect,
        DESCR,
        Debt.FIVE_MINS,
    )

    private val banned by lazy {
        val bannedPropertiesList = valueOrDefault("bannedProperties", "")
        if (bannedPropertiesList.contentEquals("")) {
            listOf()
        } else {
            bannedPropertiesList.split(",")
        }
    }

    override fun visitDotQualifiedExpression(expression: KtDotQualifiedExpression) {
        super.visitDotQualifiedExpression(expression)
        val possiblyBannedPropertyAccess = expression.node.chars

        // Does the current property access, a.b.c.d.possibly.Banned, end with
        // it. Use endsWith() so that package qualification does not interfere
        // with the check. In other words, if the configuration tells this rule
        // to ban d.definitely.Banned, flag a use of the property via
        // x.y.z.d.definitely.Banned.
        banned.filter { possiblyBannedPropertyAccess.endsWith(it) }.map {
            CodeSmell(
                issue,
                Entity.from(expression),
                "Using $possiblyBannedPropertyAccess is not allowed because accessing property $it is against Mozilla policy. See 'mozilla-detekt-rules' stanza in 'config/detekt.yml' for more information.\n",
            )
        }.forEach { report(it) }
    }
}

internal const val DESCR = "Using banned property"
