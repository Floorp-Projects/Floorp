/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.content.res.Configuration
import android.content.res.Resources
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import android.text.SpannableString
import android.text.Spanned.SPAN_EXCLUSIVE_EXCLUSIVE
import androidx.annotation.StringRes
import java.util.Formatter
import kotlin.collections.ArrayList
import kotlin.collections.HashMap

// Credit to Michael Spitsin https://medium.com/@programmerr47/working-with-spans-in-android-ca4ab1327bc4
@Suppress("SpreadOperator")
fun Resources.getSpannable(@StringRes id: Int, spanParts: List<Pair<Any, Iterable<Any>>>): CharSequence {
    val resultCreator = SpannableStringCreator()
    Formatter(
        SpannableAppendable(resultCreator, spanParts),
        getLocale(configuration)
    ).format(getString(id), *spanParts.map { it.first }.toTypedArray())
    return resultCreator.toSpannableString()
}

private fun getLocale(configuration: Configuration) =
    if (SDK_INT >= Build.VERSION_CODES.N) {
        configuration.locales[0]
    } else {
        @Suppress("Deprecation")
        configuration.locale
    }

class SpannableStringCreator {
    private val parts = ArrayList<CharSequence>()
    private var length = 0
    private val spanMap: MutableMap<IntRange, Iterable<Any>> = HashMap()

    fun append(newText: CharSequence, spans: Iterable<Any>) = apply {
        val end = newText.length
        parts.add(newText)
        spanMap[(length..length + end)] = spans
        length += end
    }

    fun append(newText: CharSequence) = apply {
        parts.add(newText)
        length += newText.length
    }

    fun toSpannableString() = SpannableString(parts.joinToString("")).apply {
        spanMap.forEach { entry ->
            val range = entry.key
            entry.value.forEach {
                setSpan(it, range.first, range.last, SPAN_EXCLUSIVE_EXCLUSIVE)
            }
        }
    }
}

class SpannableAppendable(
    private val creator: SpannableStringCreator,
    spanParts: List<Pair<Any, Iterable<Any>>>
) : Appendable {

    private val spansMap = spanParts.toMap().mapKeys { entry -> entry.key.let { it as? CharSequence ?: it.toString() } }

    override fun append(csq: CharSequence?) = apply { creator.appendSmart(csq, spansMap) }

    override fun append(csq: CharSequence?, start: Int, end: Int) = apply {
        if (csq != null) {
            if (start in 0 until end && end <= csq.length) {
                append(csq.subSequence(start, end))
            } else {
                throw IndexOutOfBoundsException("start " + start + ", end " + end + ", s.length() " + csq.length)
            }
        }
    }

    override fun append(c: Char) = apply { creator.append(c.toString()) }

    private fun SpannableStringCreator.appendSmart(csq: CharSequence?, spanDict: Map<CharSequence, Iterable<Any>>) {
        if (csq != null) {
            if (csq in spanDict) {
                append(csq, spanDict.getValue(csq))
            } else {
                val possibleMatchDict = spanDict.filter { it.key.toString() == csq }
                if (possibleMatchDict.isNotEmpty()) {
                    val spanDictEntry = possibleMatchDict.entries.toList()[0]
                    append(spanDictEntry.key, spanDictEntry.value)
                } else {
                    append(csq)
                }
            }
        }
    }
}
