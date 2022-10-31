/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.tooling.detekt

import io.gitlab.arturbosch.detekt.api.Config
import io.gitlab.arturbosch.detekt.api.RuleSet
import io.gitlab.arturbosch.detekt.api.RuleSetProvider

/**
 * Set of custom mozilla rules to be loaded in detekt utility.
 */
class MozillaRuleSetProvider : RuleSetProvider {

    override val ruleSetId = "mozilla-rules"

    override fun instance(config: Config) = RuleSet(
        ruleSetId,
        listOf(
            ProjectLicenseRule(config),
        ),
    )
}
