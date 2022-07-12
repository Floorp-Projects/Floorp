/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.content.Intent
import androidx.navigation.NavDirections
import androidx.navigation.NavOptions
import io.mockk.Matcher
import io.mockk.MockKMatcherScope
import io.mockk.internalSubstitute
import mozilla.components.support.ktx.android.os.contentEquals

/**
 * Verify that an equal [NavDirections] object was passed in a MockK verify call.
 */
fun MockKMatcherScope.directionsEq(value: NavDirections) = match(EqNavDirectionsMatcher(value))

/**
 * Verify that an equal [NavOptions] object was passed in a MockK verify call.
 */
fun MockKMatcherScope.optionsEq(value: NavOptions) = match(EqNavOptionsMatcher(value))

/**
 * Verify that two intents are the same for the purposes of intent resolution (filtering).
 * Checks if their action, data, type, identity, class, and categories are the same.
 * Does not compare extras.
 */
fun MockKMatcherScope.intentFilterEq(value: Intent) = match(EqIntentFilterMatcher(value))

private data class EqNavDirectionsMatcher(private val value: NavDirections) : Matcher<NavDirections> {

    override fun match(arg: NavDirections?): Boolean =
        value.actionId == arg?.actionId && value.arguments contentEquals arg.arguments

    override fun substitute(map: Map<Any, Any>) =
        copy(value = value.internalSubstitute(map))
}

private data class EqNavOptionsMatcher(private val value: NavOptions) : Matcher<NavOptions> {

    override fun match(arg: NavOptions?): Boolean =
        value.popUpToId == arg?.popUpToId && value.isPopUpToInclusive() == arg.isPopUpToInclusive()

    override fun substitute(map: Map<Any, Any>) =
        copy(value = value.internalSubstitute(map))
}

private data class EqIntentFilterMatcher(private val value: Intent) : Matcher<Intent> {

    override fun match(arg: Intent?): Boolean = value.filterEquals(arg)

    override fun substitute(map: Map<Any, Any>) =
        copy(value = value.internalSubstitute(map))

    override fun toString() = "intentFilterEq($value)"
}
