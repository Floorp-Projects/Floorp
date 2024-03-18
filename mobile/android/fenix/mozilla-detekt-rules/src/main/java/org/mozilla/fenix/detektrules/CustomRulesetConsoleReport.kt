/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.detektrules

import io.gitlab.arturbosch.detekt.api.ConsoleReport
import io.gitlab.arturbosch.detekt.api.Detektion

/**
 * A reporter that custom formats violations of our custom lint rules.
 */
class CustomRulesetConsoleReport : ConsoleReport() {
    @Suppress("DEPRECATION") // locationAsString
    override fun render(detektion: Detektion): String? {
        return detektion.findings["mozilla-detekt-rules"]?.fold("") { accumulator, finding ->
            accumulator + "${finding.id}:\n    ${finding.file}\n    ${finding.messageOrDescription()}\n\n"
            // This creates an extra newline at the very end but it's not worth fixing.
        }
    }
}
