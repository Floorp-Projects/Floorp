/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.content.res

import android.content.res.Resources
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import android.text.SpannableStringBuilder
import android.text.Spanned.SPAN_EXCLUSIVE_EXCLUSIVE
import android.text.SpannedString
import androidx.annotation.StringRes
import java.util.Formatter
import java.util.Locale

/**
 * Returns the primary locale according to the user's preference.
 */
val Resources.locale: Locale
    get() = if (SDK_INT >= Build.VERSION_CODES.N) {
        configuration.locales[0]
    } else {
        @Suppress("Deprecation")
        configuration.locale
    }

/**
 * Returns the character sequence associated with a given resource [id],
 * substituting format arguments with additional styling spans.
 *
 * Credit to Michael Spitsin https://medium.com/@programmerr47/working-with-spans-in-android-ca4ab1327bc4
 *
 * @param id The desired resource identifier, corresponding to a string resource.
 * @param spanParts The format arguments that will be used for substitution.
 * The first element of each pair is the text to insert, similar to [String.format].
 * The second element of each pair is a span that will be used to style the inserted string.
 */
@Suppress("SpreadOperator")
fun Resources.getSpanned(
    @StringRes id: Int,
    vararg spanParts: Pair<Any, Any>,
): SpannedString {
    val builder = SpannableStringBuilder()
    val formatArgs = spanParts.map { (text) -> text }.toTypedArray()
    val formatter = Formatter(SpannableAppendable(builder, spanParts), locale)
    formatter.format(getString(id), *formatArgs)
    return SpannedString(builder)
}

/**
 * [Appendable] implementation that wraps [SpannableStringBuilder]
 * and inserts spans from the span parts array.
 */
private class SpannableAppendable(
    private val builder: SpannableStringBuilder,
    spanParts: Array<out Pair<Any, Any>>,
) : Appendable {

    /**
     * Map of values from span parts, with keys converted to char sequences.
     */
    private val spansMap = spanParts
        .toMap()
        .mapKeys { (key) -> key.let { it as? CharSequence ?: it.toString() } }

    override fun append(csq: CharSequence?) = apply { appendSmart(csq) }

    override fun append(csq: CharSequence?, start: Int, end: Int) = apply {
        if (csq != null) {
            if (start in 0 until end && end <= csq.length) {
                append(csq.subSequence(start, end))
            } else {
                throw IndexOutOfBoundsException("start " + start + ", end " + end + ", s.length() " + csq.length)
            }
        }
    }

    override fun append(c: Char) = apply { builder.append(c.toString()) }

    /**
     * Tries to find [csq] in the [spansMap] and use the corresponding span value.
     * If [csq] is not found, the map is searched manually by converting values to strings.
     * If no match is found afterwards, [csq] is appended with no corresponding span.
     */
    private fun appendSmart(csq: CharSequence?) {
        if (csq != null) {
            if (csq in spansMap) {
                val span = spansMap.getValue(csq)
                builder.append(csq, span, SPAN_EXCLUSIVE_EXCLUSIVE)
            } else {
                val possibleMatchDict = spansMap.filter { (text) -> text.toString() == csq }
                if (possibleMatchDict.isNotEmpty()) {
                    val spanDictEntry = possibleMatchDict.entries.first()
                    builder.append(spanDictEntry.key, spanDictEntry.value, SPAN_EXCLUSIVE_EXCLUSIVE)
                } else {
                    builder.append(csq)
                }
            }
        }
    }
}
