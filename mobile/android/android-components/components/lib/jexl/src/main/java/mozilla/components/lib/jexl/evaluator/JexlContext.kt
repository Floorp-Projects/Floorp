/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.jexl.evaluator

import mozilla.components.lib.jexl.value.JexlValue

/**
 * Variables defined in the [JexlContext] are available to expressions.
 *
 * Example context:
 * <code>
 *  val context = JexlContext(
 *      "employees" to JexlArray(
 *          JexlObject(
 *              "first" to "Sterling".toJexl(),
 *              "last" to "Archer".toJexl(),
 *              "age" to 36.toJexl()),
 *          JexlObject(
 *              "first" to "Malory".toJexl(),
 *              "last" to "Archer".toJexl(),
 *              "age" to 75.toJexl()),
 *          JexlObject(
 *              "first" to "Malory".toJexl(),
 *              "last" to "Archer".toJexl(),
 *              "age" to 33.toJexl())
 *      )
 *  )
 * </code>
 *
 * This context can be accessed in an JEXL expression like this:
 *
 * <code>
 *     employees[.age >= 30 && .age < 90][.age < 35]
 * </code>
 */
class JexlContext(
    vararg pairs: Pair<String, JexlValue>,
) {
    private val properties: MutableMap<String, JexlValue> = pairs.toMap().toMutableMap()

    fun set(key: String, value: JexlValue) {
        properties[key] = value
    }

    fun get(key: String): JexlValue {
        return properties[key]
            ?: throw EvaluatorException("$key is undefined")
    }
}
