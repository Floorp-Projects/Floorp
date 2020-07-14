package org.mozilla.fenix.ext

import androidx.navigation.NavDirections
import io.mockk.Matcher
import io.mockk.MockKMatcherScope
import io.mockk.internalSubstitute
import mozilla.components.support.ktx.android.os.contentEquals

/**
 * Verify that an equal [NavDirections] object was passed in a MockK verify call.
 */
fun MockKMatcherScope.directionsEq(value: NavDirections) = match(EqNavDirectionsMatcher(value))

private data class EqNavDirectionsMatcher(private val value: NavDirections) : Matcher<NavDirections> {

    override fun match(arg: NavDirections?): Boolean =
        value.actionId == arg?.actionId && value.arguments contentEquals arg.arguments

    override fun substitute(map: Map<Any, Any>) =
        copy(value = value.internalSubstitute(map))
}
