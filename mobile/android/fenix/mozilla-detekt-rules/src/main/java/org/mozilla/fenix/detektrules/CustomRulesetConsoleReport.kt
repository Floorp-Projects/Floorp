package org.mozilla.fenix.detektrules

import io.gitlab.arturbosch.detekt.api.ConsoleReport
import io.gitlab.arturbosch.detekt.api.Detektion

class CustomRulesetConsoleReport : ConsoleReport() {
    @Suppress("DEPRECATION")
    override fun render(detektion: Detektion): String? {
        return detektion.findings["mozilla-detekt-rules"]?.fold("") { output, finding ->
            output + finding.locationAsString + ": " + finding.messageOrDescription()
        }
    }
}