/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.jexl.value

import mozilla.components.lib.jexl.evaluator.EvaluatorException

/**
 * A JEXL value type.
 */
sealed class JexlValue {
    abstract val value: Any

    abstract operator fun plus(other: JexlValue): JexlValue
    abstract operator fun times(other: JexlValue): JexlValue
    abstract operator fun div(other: JexlValue): JexlValue
    abstract operator fun compareTo(other: JexlValue): Int

    abstract fun toBoolean(): Boolean
}

/**
 * JEXL Integer type.
 */
class JexlInteger(override val value: Int) : JexlValue() {
    override fun div(other: JexlValue): JexlValue {
        return when (other) {
            is JexlInteger -> JexlInteger(value / other.value)
            is JexlDouble -> JexlDouble(value / other.value)
            else -> throw EvaluatorException("Can't divide by ${other::class}")
        }
    }

    override fun times(other: JexlValue): JexlValue {
        return when (other) {
            is JexlInteger -> JexlInteger(value * other.value)
            is JexlDouble -> JexlDouble(value * other.value)
            is JexlBoolean -> JexlInteger(value * other.toInt())
            else -> throw EvaluatorException("Can't multiply with ${other::class}")
        }
    }

    override fun plus(other: JexlValue): JexlValue {
        return when (other) {
            is JexlInteger -> JexlInteger(value + other.value)
            is JexlDouble -> JexlDouble(value + other.value)
            is JexlString -> JexlString(value.toString() + other.value)
            is JexlBoolean -> JexlInteger(value + (other.toInt()))
            else -> throw EvaluatorException("Can't add ${other::class}")
        }
    }

    override fun compareTo(other: JexlValue): Int {
        return when (other) {
            is JexlInteger -> value.compareTo(other.value)
            is JexlDouble -> value.compareTo(other.value)
            else -> throw EvaluatorException("Can't compare ${other::class}")
        }
    }

    override fun equals(other: Any?): Boolean {
        return when (other) {
            is JexlInteger -> value == other.value
            is JexlDouble -> value.toDouble() == other.value
            else -> false
        }
    }

    override fun toBoolean(): Boolean = value != 0

    override fun toString() = value.toString()

    override fun hashCode() = value.hashCode()
}

/**
 * JEXL Double type.
 */
class JexlDouble(override val value: Double) : JexlValue() {
    override fun div(other: JexlValue): JexlValue {
        return when (other) {
            is JexlInteger -> JexlDouble(value / other.value)
            is JexlDouble -> JexlDouble(value / other.value)
            else -> throw EvaluatorException("Can't divide by ${other::class}")
        }
    }

    override fun times(other: JexlValue): JexlValue {
        return when (other) {
            is JexlInteger -> JexlDouble(value * other.value)
            is JexlDouble -> JexlDouble(value * other.value)
            is JexlBoolean -> JexlDouble(value * other.toInt())
            else -> throw EvaluatorException("Can't multiply with ${other::class}")
        }
    }

    override fun plus(other: JexlValue): JexlValue {
        return when (other) {
            is JexlInteger -> JexlDouble(value + other.value)
            is JexlDouble -> JexlDouble(value + other.value)
            is JexlString -> JexlString(value.toString() + other.value)
            is JexlBoolean -> JexlDouble(value + (other.toInt()))
            else -> throw EvaluatorException("Can't add ${other::class}")
        }
    }

    override fun compareTo(other: JexlValue): Int {
        return when (other) {
            is JexlInteger -> value.compareTo(other.value)
            is JexlDouble -> value.compareTo(other.value)
            else -> throw EvaluatorException("Can't compare ${other::class}")
        }
    }

    override fun toBoolean(): Boolean = value != 0.0

    override fun equals(other: Any?): Boolean {
        return when (other) {
            is JexlDouble -> value == other.value
            is JexlInteger -> {
                value == other.value.toDouble()
            }
            else -> false
        }
    }

    override fun toString() = value.toString()

    override fun hashCode() = value.hashCode()
}

/**
 * JEXL Boolean type.
 */
class JexlBoolean(override val value: Boolean) : JexlValue() {
    override fun div(other: JexlValue): JexlValue {
        throw EvaluatorException("Can't divide boolean")
    }

    override fun times(other: JexlValue): JexlValue {
        return when (other) {
            is JexlInteger -> JexlInteger(toInt() * other.value)
            is JexlDouble -> JexlDouble(toInt() * other.value)
            is JexlBoolean -> JexlInteger(toInt() * other.toInt())
            else -> throw EvaluatorException("Can't multiply with ${other::class}")
        }
    }

    override fun plus(other: JexlValue): JexlValue {
        return when (other) {
            is JexlInteger -> JexlInteger((toInt()) + other.value)
            is JexlDouble -> JexlDouble((toInt()) + other.value)
            is JexlString -> JexlString(value.toString() + other.value)
            is JexlBoolean -> JexlInteger((toInt()) + (other.toInt()))
            else -> throw EvaluatorException("Can't add ${other::class}")
        }
    }

    override fun compareTo(other: JexlValue): Int {
        throw EvaluatorException("Can't compare ${other::class}")
    }

    fun toInt(): Int = if (value) 1 else 0

    override fun equals(other: Any?) = other is JexlBoolean && value == other.value

    override fun toBoolean() = value

    override fun toString() = value.toString()

    override fun hashCode() = value.hashCode()
}

/**
 * JEXL String type.
 */
class JexlString(override val value: String) : JexlValue() {
    override fun div(other: JexlValue): JexlValue {
        throw EvaluatorException("Can't divide string")
    }

    override fun times(other: JexlValue): JexlValue {
        throw EvaluatorException("Can't multiply strings")
    }

    override fun plus(other: JexlValue): JexlValue {
        return when (other) {
            is JexlInteger -> JexlString(value + other.value)
            is JexlDouble -> JexlString(value + other.value)
            is JexlString -> JexlString(value + other.value)
            is JexlBoolean -> JexlString(value + (if (other.value) 1 else 0))
            else -> throw EvaluatorException("Can't add ${other::class}")
        }
    }

    override fun compareTo(other: JexlValue): Int {
        throw EvaluatorException("Can't compare ${other::class}")
    }

    override fun equals(other: Any?): Boolean {
        return other is JexlString && value == other.value
    }

    override fun toBoolean(): Boolean {
        return value.isNotEmpty()
    }

    override fun toString(): String {
        return value
    }

    override fun hashCode(): Int {
        return value.hashCode()
    }
}

/**
 * JEXL Array type.
 */
class JexlArray(
    override val value: List<JexlValue>,
) : JexlValue() {
    constructor(vararg elements: JexlValue) : this(elements.toList())

    override fun div(other: JexlValue): JexlValue = throw EvaluatorException("Can't divide array")

    override fun times(other: JexlValue): JexlValue = throw EvaluatorException("Can't multiply arrays")

    override fun plus(other: JexlValue): JexlValue = throw EvaluatorException("Can't add arrays")

    override fun compareTo(other: JexlValue): Int = throw EvaluatorException("Can't compare ${other::class}")

    override fun equals(other: Any?) = other is JexlArray && value == other.value

    override fun toBoolean(): Boolean = throw EvaluatorException("Can't convert array to boolean")

    override fun toString(): String = value.toString()

    override fun hashCode(): Int = value.hashCode()
}

/**
 * JEXL Object type.
 */
class JexlObject(
    override val value: Map<String, JexlValue>,
) : JexlValue() {
    constructor(vararg pairs: Pair<String, JexlValue>) : this(pairs.toMap())

    override fun div(other: JexlValue): JexlValue {
        throw EvaluatorException("Can't divide object")
    }

    override fun times(other: JexlValue): JexlValue {
        throw EvaluatorException("Can't multiply objects")
    }

    override fun plus(other: JexlValue): JexlValue {
        throw EvaluatorException("Can't add objects")
    }

    override fun compareTo(other: JexlValue): Int {
        throw EvaluatorException("Can't compare ${other::class}")
    }

    override fun equals(other: Any?): Boolean {
        return other is JexlObject && value == other.value
    }

    override fun toBoolean(): Boolean {
        throw EvaluatorException("Can't convert object to boolean")
    }

    override fun toString(): String {
        return value.toString()
    }

    override fun hashCode(): Int {
        return value.hashCode()
    }
}

/**
 * JEXL undefined type.
 */
class JexlUndefined : JexlValue() {
    override val value = Any()

    override fun plus(other: JexlValue): JexlValue {
        return this
    }

    override fun times(other: JexlValue): JexlValue = throw EvaluatorException("Can't multiply undefined values")

    override fun div(other: JexlValue): JexlValue = throw EvaluatorException("Can't divide undefined values")

    override fun compareTo(other: JexlValue) = if (other is JexlUndefined) 0 else 1

    override fun toBoolean(): Boolean = throw EvaluatorException("Can't convert undefined to boolean")

    override fun toString() = "<undefined>"

    override fun equals(other: Any?) = other is JexlUndefined

    override fun hashCode(): Int = 7
}
