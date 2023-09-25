/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.nimbus

import org.mozilla.experiments.nimbus.GleanPlumbMessageHelper
import org.mozilla.experiments.nimbus.internal.NimbusException

/**
 * Extension method that returns true when the condition is evaluated to true, and false otherwise
 * @param condition The condition given as String.
 */
fun GleanPlumbMessageHelper.evalJexlSafe(
    condition: String,
) = try {
    evalJexl(condition)
} catch (e: NimbusException.EvaluationException) {
    false
}
